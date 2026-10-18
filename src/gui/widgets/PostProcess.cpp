#include "PostProcess.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>
#include <omp.h>

#include "CameraTypes.h"

namespace PostProcess
{

namespace
{

// 闭运算的结构元素：索引差不超过 3（中间最多隔 2 个亚阈点）的候选并成一段
constexpr int kBridgeGap = 3;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// despike 的中间结果都放这里复用。谱线每帧要过两遍，而 QVector 的下标访问
// 在 Debug 构建里带边界断言，热路径上改用 std::vector 后，6000 点谱线在
// Debug 下实测从约 18 ms 降到约 4 ms（Release 约 1.5 ms）
struct Scratch
{
    std::vector<double> diff;
    std::vector<double> medianBuf;
    std::vector<double> z;
    std::vector<unsigned char> candidate;
    std::vector<unsigned char> replacement;
    std::vector<int> span;
    std::vector<double> prefixSum;
    std::vector<double> prefixCount;
};

// 求已装好样本的 scratch 的中位数。nth_element 只保证第 mid 位就位，比排序便宜
double medianOfScratch(std::vector<double> &scratch)
{
    if (scratch.empty()) {
        return 0.0;
    }

    const int n = static_cast<int>(scratch.size());
    const int mid = n / 2;
    std::nth_element(scratch.begin(), scratch.begin() + mid, scratch.end());
    const double upper = scratch[mid];
    if (n % 2 != 0) {
        return upper;
    }

    std::nth_element(scratch.begin(), scratch.begin() + mid - 1, scratch.begin() + mid);
    return 0.5 * (scratch[mid - 1] + upper);
}

// 中位数。值复制进 medianBuf 再重排，入参不受打乱影响；medianBuf 由调用方
// 复用，避免每次求中位数都新申请一块 6000 点的内存
double medianOf(const double *src, int n, std::vector<double> &medianBuf)
{
    medianBuf.assign(src, src + n);
    return medianOfScratch(medianBuf);
}

// |src| 的中位数。不另建一份绝对值数组，边复制边取绝对值
double medianOfAbs(const double *src, int n, std::vector<double> &medianBuf)
{
    medianBuf.resize(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        medianBuf[static_cast<std::size_t>(i)] = std::abs(src[i]);
    }
    return medianOfScratch(medianBuf);
}

// 一次差分的修正 Z 分数写进 s.z，第 0 个点没有差分项，记 NaN：与任何阈值
// 比较都是 false，它既不会成为候选，也不会进闭运算的游程。
//
// MAD 为 0 表示一半以上的差分恰好相等（平坦基线、常量或严格线性的谱），
// 此时没有可用的尺度估计。按参考实现的语义把非零差分记作 ±inf、零差分记作
// NaN：即「任何非零跳变都算离群」，平坦基线上的单点尖峰因此仍能被抓到，
// 常量谱也仍然一个点都不标记
void computeModifiedZ(const QVector<double> &y, Scratch &s)
{
    const int n = y.size();
    s.z.assign(static_cast<std::size_t>(n), kNaN);
    if (n < 2) {
        return;
    }

    const double *values = y.constData();
    const int m = n - 1;
    s.diff.resize(static_cast<std::size_t>(m));
    for (int i = 0; i < m; ++i) {
        s.diff[static_cast<std::size_t>(i)] = values[i + 1] - values[i];
    }

    const double offset = medianOf(s.diff.data(), m, s.medianBuf);
    for (int i = 0; i < m; ++i) {
        s.diff[static_cast<std::size_t>(i)] -= offset;
    }

    // MAD 是「减掉中位数之后取绝对值」的中位数，少一步 std::abs 的话
    // mad 会算成有符号中位数（≈0）
    const double mad = medianOfAbs(s.diff.data(), m, s.medianBuf);
    if (mad > 0.0) {
        const double scale = 0.6745 / mad;
        for (int i = 0; i < m; ++i) {
            s.z[static_cast<std::size_t>(i + 1)] = s.diff[static_cast<std::size_t>(i)] * scale;
        }
        return;
    }

    const double inf = std::numeric_limits<double>::infinity();
    for (int i = 0; i < m; ++i) {
        const double d = s.diff[static_cast<std::size_t>(i)];
        s.z[static_cast<std::size_t>(i + 1)] = (d == 0.0) ? kNaN : std::copysign(inf, d);
    }
}

// 一维闭运算（先膨胀再腐蚀）后每个位置所在那一段的跨度。索引差不超过
// kBridgeGap 的候选并成一段，跨度取段内最左与最右候选的索引差加 1；
// 非候选处为 0。
//
// 取跨度而不是候选个数：单像素尖峰 0 +A -A 0 的两个差分本来就相邻，跨度必为 2；
// 两像素宽的亮斑 0 +A +a 0 跨度已经是 3，按候选个数算会和尖峰混成同一个值。
//
// 不按符号切开：「多宽」和「往哪边」是两件事，按符号切会让一个 +- 脉冲变成
// 两段各长 1，而下面带符号的判据正好把这两段说成一个跨度 2 的脉冲
void computeRunSpans(const std::vector<unsigned char> &candidate, std::vector<int> &span)
{
    const int n = static_cast<int>(candidate.size());
    span.assign(static_cast<std::size_t>(n), 0);

    int runStart = -1;
    int lastCandidate = -1;
    auto closeRun = [&span](int start, int end) {
        const int runSpan = end - start + 1;
        for (int p = start; p <= end; ++p) {
            span[static_cast<std::size_t>(p)] = runSpan;
        }
    };

    for (int i = 0; i < n; ++i) {
        if (candidate[static_cast<std::size_t>(i)] == 0) {
            continue;
        }
        if (runStart < 0) {
            runStart = i;
        } else if (i - lastCandidate > kBridgeGap) {
            closeRun(runStart, lastCandidate);
            runStart = i;
        }
        lastCandidate = i;
    }
    if (runStart >= 0) {
        closeRun(runStart, lastCandidate);
    }
}

// 求待替换的尖峰掩膜，返回置位的个数。差分形状：尖峰的差分必须是
// 0, +A, -A, 0，也就是两个相邻的超阈差分，一正一负，且所在游程的跨度恰好为 2。
//
// 次阈值判据要求尖峰外侧的两个差分 z[p-1] 和 z[p+2] 都不超过它：尖峰占掉
// z[p] 和 z[p+1] 这一对差分，剩下的这两个才是它左右的背景；真宇宙线的邻居
// 就是背景，差分很小，谱带被主阈值削剩的肩部却仍留在邻居里
int computeReplacement(const std::vector<double> &z, const DespikeParams &params, Scratch &s)
{
    const int n = static_cast<int>(z.size());
    s.candidate.assign(static_cast<std::size_t>(n), 0);
    for (int i = 0; i < n; ++i) {
        s.candidate[static_cast<std::size_t>(i)] =
            (std::abs(z[static_cast<std::size_t>(i)]) > params.primaryThreshold) ? 1 : 0;
    }

    computeRunSpans(s.candidate, s.span);

    const bool useSecondary = params.secondaryThreshold > 0.0;
    s.replacement.assign(static_cast<std::size_t>(n), 0);

    int count = 0;
    // 末位没有后继差分，形状判据天然不会命中它
    for (int p = 0; p + 1 < n; ++p) {
        // 跨度 2 且先正后负，等价于原来「带符号跨度 sw[p]==2 且 sw[p+1]==-2」
        if (s.span[static_cast<std::size_t>(p)] != 2
            || s.span[static_cast<std::size_t>(p + 1)] != 2) {
            continue;
        }
        if (z[static_cast<std::size_t>(p)] <= 0.0
            || z[static_cast<std::size_t>(p + 1)] >= 0.0) {
            continue;
        }
        if (useSecondary) {
            const bool leftHot = (p >= 1)
                && std::abs(z[static_cast<std::size_t>(p - 1)]) > params.secondaryThreshold;
            const bool rightHot = (p + 2 < n)
                && std::abs(z[static_cast<std::size_t>(p + 2)]) > params.secondaryThreshold;
            if (leftHot || rightHot) {
                continue;
            }
        }
        s.replacement[static_cast<std::size_t>(p)] = 1;
        ++count;
    }
    return count;
}

} // namespace

DespikeResult despike(const QVector<double> &y, const DespikeParams &paramsIn)
{
    DespikeParams params = paramsIn;
    if (params.windowHalfWidth < 1) {
        params.windowHalfWidth = 1;
    }
    if (params.passes < 1) {
        params.passes = 1;
    }

    DespikeResult result;
    result.values = y;
    result.marked.fill(false, y.size());

    const int n = y.size();
    Scratch s;
    computeModifiedZ(y, s);

    // 第一遍用的就是输入数据的 Z 分数，顺手作为结果交出去，不用再算一遍
    result.zScores.resize(n);
    for (int i = 0; i < n; ++i) {
        result.zScores[i] = s.z[static_cast<std::size_t>(i)];
    }

    // 少于 3 个点凑不出「一正一负」的差分对，形状判据不可能命中
    if (n < 3) {
        return result;
    }

    const int m = params.windowHalfWidth;
    s.prefixSum.resize(static_cast<std::size_t>(n) + 1);
    s.prefixCount.resize(static_cast<std::size_t>(n) + 1);

    for (int pass = 0; pass < params.passes; ++pass) {
        if (pass > 0) {
            computeModifiedZ(result.values, s);
        }

        if (computeReplacement(s.z, params, s) == 0) {
            break;
        }

        // 均值窗口里未标记点的和与个数。前缀和在写入之前整条算完，替换值一律
        // 基于这一遍开始时的数据，所以结果和扫描顺序无关
        s.prefixSum[0] = 0.0;
        s.prefixCount[0] = 0.0;
        for (int i = 0; i < n; ++i) {
            const bool replace = s.replacement[static_cast<std::size_t>(i)] != 0;
            const double keep = replace ? 0.0 : 1.0;
            s.prefixSum[static_cast<std::size_t>(i + 1)] =
                s.prefixSum[static_cast<std::size_t>(i)] + result.values[i] * keep;
            s.prefixCount[static_cast<std::size_t>(i + 1)] =
                s.prefixCount[static_cast<std::size_t>(i)] + keep;
        }

        for (int i = 0; i < n; ++i) {
            if (s.replacement[static_cast<std::size_t>(i)] == 0) {
                continue;
            }
            const int lo = std::max(0, i - m);
            const int hi = std::min(n - 1, i + m);
            const double den = s.prefixCount[static_cast<std::size_t>(hi + 1)]
                             - s.prefixCount[static_cast<std::size_t>(lo)];
            if (den <= 0.0) {
                ++result.emptyWindowCount;
                continue;
            }
            const double num = s.prefixSum[static_cast<std::size_t>(hi + 1)]
                             - s.prefixSum[static_cast<std::size_t>(lo)];
            result.values[i] = num / den;
            result.marked[i] = true;
        }
    }

    for (const bool marked : result.marked) {
        if (marked) {
            ++result.markedCount;
        }
    }
    return result;
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

    if (frame.image.format() == QImage::Format_Grayscale16) {
        const ushort *srcData = reinterpret_cast<const ushort *>(frame.image.bits());
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
        const uchar *srcData = frame.image.bits();
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
        const uchar *srcData = frame.image.bits();
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
