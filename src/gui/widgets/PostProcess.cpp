#include "PostProcess.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>
#include <omp.h>

#include "CameraTypes.h"

namespace PostProcess
{

namespace
{

// ---------------------------------------------------------------------------
// 图像去尖峰的辅助函数
// ---------------------------------------------------------------------------

// 一行里最多取这么多相邻差分来估计噪声。6000 点宽时步长约 11，中位数估计
// 仍然足够稳；取满一行会让每行多花几倍时间，而图像每帧都要跑一遍
constexpr int kNoiseSamplesPerRow = 512;

// 标准差为 σ 的高斯白噪声，一次差分的标准差是 σ√2；而 |N(0,1)| 的中位数是
// 0.6745。于是 median(|相邻差分|) / (0.6745·√2) 就是单像素噪声尺度
constexpr double kMedianAbsNormal = 0.6744897501960817;
constexpr double kDiffToPixelSigma = 1.4142135623730951;

template <typename T>
inline T max8(T a, T b, T c, T d, T e, T f, T g, T h)
{
    return std::max(std::max(std::max(a, b), std::max(c, d)),
                    std::max(std::max(e, f), std::max(g, h)));
}

// 4 个数的中位数。判据已经保证 8 个邻居全都比中心低，取这 4 个直接邻居的
// 中值就够了，比再求一次 9 数中值便宜一半
inline int median4(int a, int b, int c, int d)
{
    const int lo = std::min(std::min(a, b), std::min(c, d));
    const int hi = std::max(std::max(a, b), std::max(c, d));
    return (a + b + c + d - lo - hi + 1) / 2;
}

// 只要上中位数，不必为偶数个样本再跑一次 nth_element。噪声尺度是估计值，
// 两个中位数之间的差别远小于阈值本身的余量
double upperMedianOfScratch(std::vector<double> &scratch)
{
    if (scratch.empty()) {
        return 0.0;
    }
    const std::size_t mid = scratch.size() / 2;
    std::nth_element(scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(mid),
                     scratch.end());
    return scratch[mid];
}

// 用一行的相邻差分中位数估计这一行的噪声尺度。stride 是通道间距：
// 灰度图取 1，RGB888 取 3（只采样绿通道，三个通道噪声水平本来就接近）
template <typename Pixel>
double estimateRowSigma(const Pixel *row, int width, int stride, std::vector<double> &buf)
{
    buf.clear();
    if (width < 2) {
        return 0.0;
    }

    const int step = std::max(1, width / kNoiseSamplesPerRow);
    buf.reserve(static_cast<std::size_t>((width - 1) / step + 1));
    for (int x = 0; x + 1 < width; x += step) {
        buf.push_back(std::abs(static_cast<double>(row[(x + 1) * stride])
                               - static_cast<double>(row[x * stride])));
    }

    if (buf.empty()) {
        return 0.0;
    }
    return upperMedianOfScratch(buf) / (kMedianAbsNormal * kDiffToPixelSigma);
}

// 中心像素严格高于全部 8 个邻居、且高出量超过阈值时，把它替换成直接邻居的中值。
// 阈值已经预先取整成像素值，热路径上不再做 double 比较
template <typename Pixel>
bool replaceIsolatedPixel(const Pixel *prev, const Pixel *cur, const Pixel *next,
                          int index, int stride, Pixel *out, int threshold)
{
    const int center = static_cast<int>(cur[index]);
    const int highestNeighbour = max8(
        static_cast<int>(prev[index - stride]), static_cast<int>(prev[index]),
        static_cast<int>(prev[index + stride]),
        static_cast<int>(cur[index - stride]), static_cast<int>(cur[index + stride]),
        static_cast<int>(next[index - stride]), static_cast<int>(next[index]),
        static_cast<int>(next[index + stride]));

    const int gap = center - highestNeighbour;
    if (gap <= 0 || gap < threshold) {
        return false;
    }

    out[index] = static_cast<Pixel>(median4(static_cast<int>(cur[index - stride]),
                                            static_cast<int>(cur[index + stride]),
                                            static_cast<int>(prev[index]),
                                            static_cast<int>(next[index])));
    return true;
}

// 把 primaryThreshold·sigma 换成整数门限。gap 是整数，floor+1 之后
// 「gap >= 门限」与「gap > 实数门限」完全等价（例如实数门限 40.0 要的是 41）；
// sigma 为 0 时门限为 1，任何严格大于 0 的跳变都算离群
inline int thresholdFromSigma(const DespikeParams &params, double sigma)
{
    const double threshold = params.primaryThreshold * sigma;
    if (!(threshold > 0.0)) {
        return 1;
    }
    return static_cast<int>(std::floor(threshold)) + 1;
}

// 整幅图都写一遍（没改动的像素照抄原值），而不是先 copy() 一份再改几个点。
// QImage::copy() 在 6000x4000 上要 50 ms 以上，而复制和检测是两次完整的内存
// 往返；直接分配目标图、一遍写完把两次往返合成一次，实测总耗时约减半。
//
// firstRow / lastRow 是已经夹紧过的 0-based 闭区间。范围外的行只做整行复制，
// 不参与检测，也不补做去尖峰。
template <typename Pixel>
int despikeGrayImage(const QImage &source, QImage &target, const DespikeParams &params,
                     int firstRow, int lastRow)
{
    const int width = source.width();
    const int height = source.height();
    const int rowBytes = width * static_cast<int>(sizeof(Pixel));
    // QImage 的行可能带尾部填充。只写 width 个像素会留下未初始化的填充字节，
    // 所以行尾也照抄一份（通常 0 或 2 字节，代价可以忽略）
    const int bytesPerLine = source.bytesPerLine();
    int marked = 0;

    #pragma omp parallel
    {
        std::vector<double> noiseBuf;
        noiseBuf.reserve(static_cast<std::size_t>(kNoiseSamplesPerRow + 1));
        // 每行的 8 邻居最大值。分成两遍：第一遍没有分支，编译器能向量化，
        // 第二遍只做整数的比较与少量替换
        std::vector<Pixel> neighbourMax(static_cast<std::size_t>(width));

        #pragma omp for schedule(static) reduction(+:marked)
        for (int y = 0; y < height; ++y) {
            const Pixel *cur = reinterpret_cast<const Pixel *>(source.constScanLine(y));
            Pixel *out = reinterpret_cast<Pixel *>(target.scanLine(y));

            // 行范围外，以及图像上下边界（没有完整的 3×3 邻域），整行照抄
            if (y < firstRow || y > lastRow || y == 0 || y == height - 1) {
                std::memcpy(out, cur, static_cast<std::size_t>(bytesPerLine));
                continue;
            }

            // 范围的首尾行照样使用范围外的相邻行做邻域，边界上的孤立点不会漏
            const Pixel *prev = reinterpret_cast<const Pixel *>(source.constScanLine(y - 1));
            const Pixel *next = reinterpret_cast<const Pixel *>(source.constScanLine(y + 1));

            for (int x = 1; x < width - 1; ++x) {
                neighbourMax[static_cast<std::size_t>(x)] = static_cast<Pixel>(
                    max8(static_cast<int>(prev[x - 1]), static_cast<int>(prev[x]),
                         static_cast<int>(prev[x + 1]),
                         static_cast<int>(cur[x - 1]), static_cast<int>(cur[x + 1]),
                         static_cast<int>(next[x - 1]), static_cast<int>(next[x]),
                         static_cast<int>(next[x + 1])));
            }

            const int threshold = thresholdFromSigma(params, estimateRowSigma(cur, width, 1, noiseBuf));

            out[0] = cur[0];
            out[width - 1] = cur[width - 1];
            for (int x = 1; x < width - 1; ++x) {
                const int gap = static_cast<int>(cur[x])
                              - static_cast<int>(neighbourMax[static_cast<std::size_t>(x)]);
                // thresholdFromSigma() 保证门限至少是 1，所以 gap >= threshold
                // 已经蕴含「严格高于全部邻居」
                if (gap >= threshold) {
                    out[x] = static_cast<Pixel>(median4(static_cast<int>(cur[x - 1]),
                                                        static_cast<int>(cur[x + 1]),
                                                        static_cast<int>(prev[x]),
                                                        static_cast<int>(next[x])));
                    ++marked;
                } else {
                    out[x] = cur[x];
                }
            }

            if (bytesPerLine > rowBytes) {
                std::memcpy(reinterpret_cast<uchar *>(out) + rowBytes,
                            reinterpret_cast<const uchar *>(cur) + rowBytes,
                            static_cast<std::size_t>(bytesPerLine - rowBytes));
            }
        }
    }
    return marked;
}

int despikeRgbImage(const QImage &source, QImage &target, const DespikeParams &params,
                    int firstRow, int lastRow)
{
    const int width = source.width();
    const int height = source.height();
    const int rowBytes = width * 3;
    const int bytesPerLine = source.bytesPerLine();
    int marked = 0;

    #pragma omp parallel
    {
        std::vector<double> noiseBuf;
        noiseBuf.reserve(static_cast<std::size_t>(kNoiseSamplesPerRow + 1));

        #pragma omp for schedule(static) reduction(+:marked)
        for (int y = 0; y < height; ++y) {
            const uchar *cur = source.constScanLine(y);
            uchar *out = target.scanLine(y);

            // 行范围外，以及图像上下边界，整行照抄
            if (y < firstRow || y > lastRow || y == 0 || y == height - 1) {
                std::memcpy(out, cur, static_cast<std::size_t>(bytesPerLine));
                continue;
            }

            const uchar *prev = source.constScanLine(y - 1);
            const uchar *next = source.constScanLine(y + 1);

            // 三个通道的噪声水平相近，用绿通道估计一次就够
            const int threshold = thresholdFromSigma(
                params, estimateRowSigma(cur + 1, width, 3, noiseBuf));

            out[0] = cur[0];
            out[1] = cur[1];
            out[2] = cur[2];
            out[rowBytes - 3] = cur[rowBytes - 3];
            out[rowBytes - 2] = cur[rowBytes - 2];
            out[rowBytes - 1] = cur[rowBytes - 1];

            for (int x = 1; x < width - 1; ++x) {
                bool replaced = false;
                for (int channel = 0; channel < 3; ++channel) {
                    const int index = x * 3 + channel;
                    if (replaceIsolatedPixel(prev, cur, next, index, 3, out, threshold)) {
                        replaced = true;
                    } else {
                        out[index] = cur[index];
                    }
                }
                if (replaced) {
                    ++marked;
                }
            }

            if (bytesPerLine > rowBytes) {
                std::memcpy(out + rowBytes, cur + rowBytes,
                            static_cast<std::size_t>(bytesPerLine - rowBytes));
            }
        }
    }
    return marked;
}

} // namespace

QImage despikeImage(const QImage &image, const DespikeParams &params, ImageDespikeResult *result,
                    int firstRow, int lastRow)
{
    ImageDespikeResult summary;
    if (result != nullptr) {
        *result = summary;
    }

    if (image.isNull() || image.width() < 3 || image.height() < 3) {
        return image;
    }

    const int height = image.height();

    // 夹紧到有效行，再把没有完整 3×3 邻域的上下边界排除掉。范围首尾之外的行
    // 原样保留，sampleCount 只统计真正参与检测的像素。
    firstRow = std::max(0, std::min(firstRow, height - 1));
    lastRow = (lastRow < 0) ? height - 1 : std::max(0, std::min(lastRow, height - 1));
    const int firstProcessed = std::max(firstRow, 1);
    const int lastProcessed = std::min(lastRow, height - 2);
    if (firstProcessed > lastProcessed) {
        return image;
    }

    summary.sampleCount = (image.width() - 2) * (lastProcessed - firstProcessed + 1);

    // 不调用 QImage::copy()：目标图的每个像素都会在下面的循环里写一遍（范围外
    // 的行整行复制），复制一份再改几个点等于多走一遍 48 MB 的读 + 写
    QImage target(image.width(), height, image.format());
    if (target.isNull()) {
        summary.sampleCount = 0;
        if (result != nullptr) {
            *result = summary;
        }
        return image;
    }

    switch (image.format()) {
        case QImage::Format_Grayscale16:
            summary.markedCount = despikeGrayImage<ushort>(image, target, params,
                                                           firstRow, lastRow);
            break;
        case QImage::Format_Grayscale8:
            summary.markedCount = despikeGrayImage<uchar>(image, target, params,
                                                          firstRow, lastRow);
            break;
        case QImage::Format_RGB888:
            summary.markedCount = despikeRgbImage(image, target, params, firstRow, lastRow);
            break;
        default:
            // 不认识的格式原样返回，也不声称处理过任何像素
            summary.sampleCount = 0;
            if (result != nullptr) {
                *result = summary;
            }
            return image;
    }

    if (result != nullptr) {
        *result = summary;
    }
    return target;
}

void verticalBinning(ImageData &frame, int startRow, int endRow)
{
    startRow = startRow - 1;
    endRow = endRow - 1;
    if (startRow < 0) startRow = 0;
    if (endRow < 0 || endRow >= frame.image.height()) endRow = frame.image.height() - 1;
    if (startRow > endRow) return;

    const int width = frame.image.width();

    if (frame.originalImage.isNull()) {
        frame.originalImage = frame.image;
    }

    QImage binnedImage(width, 1, frame.image.format());

    // 只读源图，用 constBits()：originalImage 已经和 image 共享同一份数据，
    // 调非 const 的 bits() 会触发 detach，把整幅图再复制一遍（6000x4000 约 50 ms）
    if (frame.image.format() == QImage::Format_Grayscale16) {
        const ushort *srcData = reinterpret_cast<const ushort *>(frame.image.constBits());
        ushort *dstData = reinterpret_cast<ushort *>(binnedImage.bits());
        frame.spectrum.resize(width);

        #pragma omp parallel for schedule(static)
        for (int x = 0; x < width; ++x) {
            quint64 sum = 0;
            const ushort *row = srcData + startRow * width + x;
            for (int y = startRow; y <= endRow; ++y, row += width) {
                sum += *row;
            }
            frame.spectrum[x] = sum;
        }

        quint64 maxVal = 0;
        for (int x = 0; x < width; ++x) {
            if (frame.spectrum[x] > maxVal) maxVal = frame.spectrum[x];
        }

        if (maxVal > 0 && maxVal <= 65535) {
            for (int x = 0; x < width; ++x) {
                dstData[x] = static_cast<ushort>(frame.spectrum[x]);
            }
        } else {
            const double scale = 65535.0 / maxVal;
            for (int x = 0; x < width; ++x) {
                dstData[x] = static_cast<ushort>(frame.spectrum[x] * scale);
            }
        }
    } else if (frame.image.format() == QImage::Format_Grayscale8) {
        const uchar *srcData = frame.image.constBits();
        uchar *dstData = binnedImage.bits();
        frame.spectrum.resize(width);

        #pragma omp parallel for schedule(static)
        for (int x = 0; x < width; ++x) {
            quint64 sum = 0;
            const uchar *row = srcData + startRow * width + x;
            for (int y = startRow; y <= endRow; ++y, row += width) {
                sum += *row;
            }
            frame.spectrum[x] = sum;
        }

        quint64 maxVal = 0;
        for (int x = 0; x < width; ++x) {
            if (frame.spectrum[x] > maxVal) maxVal = frame.spectrum[x];
        }

        if (maxVal > 0 && maxVal <= 255) {
            for (int x = 0; x < width; ++x) {
                dstData[x] = static_cast<uchar>(frame.spectrum[x]);
            }
        } else {
            const double scale = 255.0 / maxVal;
            for (int x = 0; x < width; ++x) {
                dstData[x] = static_cast<uchar>(frame.spectrum[x] * scale);
            }
        }
    } else if (frame.image.format() == QImage::Format_RGB888) {
        const uchar *srcData = frame.image.constBits();
        uchar *dstData = binnedImage.bits();
        const int rowStride = width * 3;
        frame.spectrum.resize(width * 3);

        #pragma omp parallel for schedule(static)
        for (int x = 0; x < width; ++x) {
            quint64 sumR = 0, sumG = 0, sumB = 0;
            const uchar *row = srcData + startRow * rowStride + x * 3;
            for (int y = startRow; y <= endRow; ++y, row += rowStride) {
                sumR += row[0];
                sumG += row[1];
                sumB += row[2];
            }
            frame.spectrum[x * 3 + 0] = sumR;
            frame.spectrum[x * 3 + 1] = sumG;
            frame.spectrum[x * 3 + 2] = sumB;
        }

        quint64 maxVal = 0;
        for (int x = 0; x < width; ++x) {
            if (frame.spectrum[x * 3 + 0] > maxVal) maxVal = frame.spectrum[x * 3 + 0];
            if (frame.spectrum[x * 3 + 1] > maxVal) maxVal = frame.spectrum[x * 3 + 1];
            if (frame.spectrum[x * 3 + 2] > maxVal) maxVal = frame.spectrum[x * 3 + 2];
        }

        if (maxVal > 0 && maxVal <= 255) {
            for (int x = 0; x < width; ++x) {
                dstData[x * 3 + 0] = static_cast<uchar>(frame.spectrum[x * 3 + 0]);
                dstData[x * 3 + 1] = static_cast<uchar>(frame.spectrum[x * 3 + 1]);
                dstData[x * 3 + 2] = static_cast<uchar>(frame.spectrum[x * 3 + 2]);
            }
        } else {
            const double scale = 255.0 / maxVal;
            for (int x = 0; x < width; ++x) {
                dstData[x * 3 + 0] = static_cast<uchar>(frame.spectrum[x * 3 + 0] * scale);
                dstData[x * 3 + 1] = static_cast<uchar>(frame.spectrum[x * 3 + 1] * scale);
                dstData[x * 3 + 2] = static_cast<uchar>(frame.spectrum[x * 3 + 2] * scale);
            }
        }
    }

    frame.image = binnedImage;
}

void applyDarkCalibration(ImageData &frame, const QImage *darkFrame, int customBias)
{
    if (darkFrame == nullptr || darkFrame->isNull()) {
        return;
    }
    if (darkFrame->size() != frame.image.size()) {
        return;
    }
    if (frame.image.isNull()) {
        return;
    }

    const int width = frame.image.width();
    const int height = frame.image.height();
    if (frame.image.format() == QImage::Format_Grayscale16) {
        const QImage darkRef = *darkFrame;
        for (int y = 0; y < height; ++y) {
            const ushort *srcRow = reinterpret_cast<const ushort *>(frame.image.constScanLine(y));
            const ushort *darkRow = reinterpret_cast<const ushort *>(darkRef.constScanLine(y));
            ushort *dstRow = reinterpret_cast<ushort *>(frame.image.scanLine(y));
            for (int x = 0; x < width; ++x) {
                int v = static_cast<int>(srcRow[x]) - static_cast<int>(darkRow[x]) + customBias;
                if (v < 0) v = 0;
                if (v > 65535) v = 65535;
                dstRow[x] = static_cast<ushort>(v);
            }
        }
    } else if (frame.image.format() == QImage::Format_Grayscale8) {
        const QImage darkRef = *darkFrame;
        for (int y = 0; y < height; ++y) {
            const uchar *srcRow = frame.image.constScanLine(y);
            const uchar *darkRow = darkRef.constScanLine(y);
            uchar *dstRow = frame.image.scanLine(y);
            for (int x = 0; x < width; ++x) {
                int v = static_cast<int>(srcRow[x]) - static_cast<int>(darkRow[x]) + customBias;
                if (v < 0) v = 0;
                if (v > 255) v = 255;
                dstRow[x] = static_cast<uchar>(v);
            }
        }
    } else if (frame.image.format() == QImage::Format_RGB888) {
        const QImage darkRef = *darkFrame;
        const int rowStride = width * 3;
        for (int y = 0; y < height; ++y) {
            const uchar *srcRow = frame.image.constScanLine(y);
            const uchar *darkRow = darkRef.constScanLine(y);
            uchar *dstRow = frame.image.scanLine(y);
            for (int x = 0; x < rowStride; ++x) {
                int v = static_cast<int>(srcRow[x]) - static_cast<int>(darkRow[x]) + customBias;
                if (v < 0) v = 0;
                if (v > 255) v = 255;
                dstRow[x] = static_cast<uchar>(v);
            }
        }
    }
}

} // namespace PostProcess
