#include "PostProcess.h"

#include <QDebug>
#include <QVector>
#include <algorithm>

namespace {

using PostProcess::Operation;
using PostProcess::ProcessConfig;

QImage applyVerticalBinning(const QImage &image, int startRow, int endRow);
QImage applyDarkFrameSubtraction(const QImage &image, const QImage &darkFrame);
QImage applyFlatFieldCorrection(const QImage &image, const QImage &flatField);

} // namespace

void PostProcess::processFrame(ImageData &frame, const ProcessConfig &config)
{
    if (!config.enabled) {
        return;
    }

    if (!frame.hasOriginal()) {
        frame.originalImage = frame.image;
    }

    if (config.operations.testFlag(VerticalBinning)) {
        int startRow = config.vBinStartRow;
        int endRow = config.vBinEndRow;

        if (endRow < 0 || endRow >= frame.originalImage.height()) {
            endRow = frame.originalImage.height() - 1;
        }

        if (startRow < 0) {
            startRow = 0;
        }

        if (startRow <= endRow) {
            frame.image = applyVerticalBinning(frame.originalImage, startRow, endRow);
        }
    }

    if (config.operations.testFlag(DarkFrameSubtraction) && !config.darkFrame.isNull()) {
        frame.image = applyDarkFrameSubtraction(frame.image, config.darkFrame);
    }

    if (config.operations.testFlag(FlatFieldCorrection) && !config.flatField.isNull()) {
        frame.image = applyFlatFieldCorrection(frame.image, config.flatField);
    }
}

namespace {

QImage applyVerticalBinning(const QImage &image, int startRow, int endRow)
{
    const int width = image.width();
    const int height = image.height();

    if (width <= 0 || height <= 0) {
        return image;
    }

    if (startRow < 0) startRow = 0;
    if (endRow >= height) endRow = height - 1;
    if (startRow > endRow) {
        return image;
    }

    int rowCount = endRow - startRow + 1;

    QImage binnedImage(width, 1, image.format());

    if (image.format() == QImage::Format_Grayscale16) {
        const ushort *srcData = reinterpret_cast<const ushort *>(image.bits());
        ushort *dstData = reinterpret_cast<ushort *>(binnedImage.bits());

        static quint64 sumsBuffer[16384];
        quint64 *sums = sumsBuffer;

        for (int x = 0; x < width; x++) {
            sums[x] = 0;
        }

        for (int y = startRow; y <= endRow; y++) {
            const ushort *row = srcData + y * width;
            for (int x = 0; x < width; x++) {
                sums[x] += row[x];
            }
        }

        for (int x = 0; x < width; x++) {
            dstData[x] = static_cast<ushort>(sums[x] / rowCount);
        }
    } else if (image.format() == QImage::Format_Grayscale8) {
        const uchar *srcData = image.bits();
        uchar *dstData = binnedImage.bits();

        static quint32 sumsBuffer[16384];
        quint32 *sums = sumsBuffer;

        for (int x = 0; x < width; x++) {
            sums[x] = 0;
        }

        for (int y = startRow; y <= endRow; y++) {
            const uchar *row = srcData + y * width;
            for (int x = 0; x < width; x++) {
                sums[x] += row[x];
            }
        }

        for (int x = 0; x < width; x++) {
            dstData[x] = static_cast<uchar>(sums[x] / rowCount);
        }
    } else if (image.format() == QImage::Format_RGB888) {
        const uchar *srcData = image.bits();
        uchar *dstData = binnedImage.bits();

        const int rowStride = width * 3;
        static quint32 sumsRBuffer[16384], sumsGBuffer[16384], sumsBBuffer[16384];
        quint32 *sumsR = sumsRBuffer;
        quint32 *sumsG = sumsGBuffer;
        quint32 *sumsB = sumsBBuffer;

        for (int x = 0; x < width; x++) {
            sumsR[x] = sumsG[x] = sumsB[x] = 0;
        }

        for (int y = startRow; y <= endRow; y++) {
            const uchar *row = srcData + y * rowStride;
            for (int x = 0; x < width; x++) {
                int idx = x * 3;
                sumsR[x] += row[idx];
                sumsG[x] += row[idx + 1];
                sumsB[x] += row[idx + 2];
            }
        }

        for (int x = 0; x < width; x++) {
            dstData[x * 3] = static_cast<uchar>(sumsR[x] / rowCount);
            dstData[x * 3 + 1] = static_cast<uchar>(sumsG[x] / rowCount);
            dstData[x * 3 + 2] = static_cast<uchar>(sumsB[x] / rowCount);
        }
    } else {
        qWarning() << "PostProcess: Unsupported image format for vertical binning";
        return image;
    }

    qDebug() << "PostProcess: Applied vertical binning:" << width << "x" << rowCount << "->" << width << "x1";
    return binnedImage;
}

QImage applyDarkFrameSubtraction(const QImage &image, const QImage &darkFrame)
{
    if (image.size() != darkFrame.size() || image.format() != darkFrame.format()) {
        qWarning() << "PostProcess: Dark frame size/format mismatch";
        return image;
    }

    QImage result = image.copy();

    if (image.format() == QImage::Format_Grayscale16) {
        const ushort *srcData = reinterpret_cast<const ushort *>(image.bits());
        const ushort *darkData = reinterpret_cast<const ushort *>(darkFrame.bits());
        ushort *dstData = reinterpret_cast<ushort *>(result.bits());
        int pixelCount = image.width() * image.height();

        for (int i = 0; i < pixelCount; i++) {
            int diff = static_cast<int>(srcData[i]) - static_cast<int>(darkData[i]);
            dstData[i] = static_cast<ushort>(qMax(0, diff));
        }
    } else if (image.format() == QImage::Format_Grayscale8) {
        const uchar *srcData = image.bits();
        const uchar *darkData = darkFrame.bits();
        uchar *dstData = result.bits();
        int pixelCount = image.width() * image.height();

        for (int i = 0; i < pixelCount; i++) {
            int diff = static_cast<int>(srcData[i]) - static_cast<int>(darkData[i]);
            dstData[i] = static_cast<uchar>(qMax(0, diff));
        }
    }

    return result;
}

QImage applyFlatFieldCorrection(const QImage &image, const QImage &flatField)
{
    if (image.size() != flatField.size() || image.format() != flatField.format()) {
        qWarning() << "PostProcess: Flat field size/format mismatch";
        return image;
    }

    QImage result = image.copy();

    if (image.format() == QImage::Format_Grayscale16) {
        const ushort *srcData = reinterpret_cast<const ushort *>(image.bits());
        const ushort *flatData = reinterpret_cast<const ushort *>(flatField.bits());
        ushort *dstData = reinterpret_cast<ushort *>(result.bits());
        int pixelCount = image.width() * image.height();

        for (int i = 0; i < pixelCount; i++) {
            if (flatData[i] > 0) {
                double corrected = (static_cast<double>(srcData[i]) / static_cast<double>(flatData[i])) * 65535.0;
                dstData[i] = static_cast<ushort>(qMin(static_cast<int>(corrected), 65535));
            } else {
                dstData[i] = 0;
            }
        }
    } else if (image.format() == QImage::Format_Grayscale8) {
        const uchar *srcData = image.bits();
        const uchar *flatData = flatField.bits();
        uchar *dstData = result.bits();
        int pixelCount = image.width() * image.height();

        for (int i = 0; i < pixelCount; i++) {
            if (flatData[i] > 0) {
                double corrected = (static_cast<double>(srcData[i]) / static_cast<double>(flatData[i])) * 255.0;
                dstData[i] = static_cast<uchar>(qMin(static_cast<int>(corrected), 255));
            } else {
                dstData[i] = 0;
            }
        }
    }

    return result;
}

} // namespace