#include "PostProcess.h"

#include "CameraTypes.h"

namespace PostProcess
{

void verticalBinning(ImageData &frame, int startRow, int endRow)
{
    startRow = startRow - 1;
    endRow = endRow - 1;
    if (startRow < 0) startRow = 0;
    if (endRow < 0 || endRow >= frame.image.height()) endRow = frame.image.height() - 1;
    if (startRow > endRow) return;

    const int width = frame.image.width();
    int rowCount = endRow - startRow + 1;

    if (frame.originalImage.isNull()) {
        frame.originalImage = frame.image;
    }

    QImage binnedImage(width, 1, frame.image.format());
    frame.spectrum.resize(width);
    frame.spectrum.fill(0.0);

    if (frame.image.format() == QImage::Format_Grayscale16) {
        const ushort *srcData = reinterpret_cast<const ushort *>(frame.image.bits());
        ushort *dstData = reinterpret_cast<ushort *>(binnedImage.bits());
        for (int y = startRow; y <= endRow; y++) {
            const ushort *row = srcData + y * width;
            for (int x = 0; x < width; x++) {
                frame.spectrum[x] += row[x];
            }
        }
        double maxVal = 0.0;
        for (int x = 0; x < width; x++) {
            if (frame.spectrum[x] > maxVal) maxVal = frame.spectrum[x];
        }
        if (maxVal > 0.0 && maxVal <= 65535.0) {
            for (int x = 0; x < width; x++) {
                dstData[x] = static_cast<ushort>(frame.spectrum[x]);
            }
        } else {
            for (int x = 0; x < width; x++) {
                dstData[x] = static_cast<ushort>(frame.spectrum[x] / maxVal * 65535.0);
            }
        }
    } else if (frame.image.format() == QImage::Format_Grayscale8) {
        const uchar *srcData = frame.image.bits();
        uchar *dstData = binnedImage.bits();
        for (int y = startRow; y <= endRow; y++) {
            const uchar *row = srcData + y * width;
            for (int x = 0; x < width; x++) {
                frame.spectrum[x] += row[x];
            }
        }
        double maxVal = 0.0;
        for (int x = 0; x < width; x++) {
            if (frame.spectrum[x] > maxVal) maxVal = frame.spectrum[x];
        }
        if (maxVal > 0.0 && maxVal <= 255.0) {
            for (int x = 0; x < width; x++) {
                dstData[x] = static_cast<uchar>(frame.spectrum[x]);
            }
        } else {
            for (int x = 0; x < width; x++) {
                dstData[x] = static_cast<uchar>(frame.spectrum[x] / maxVal * 255.0);
            }
        }
    } else if (frame.image.format() == QImage::Format_RGB888) {
        const uchar *srcData = frame.image.bits();
        uchar *dstData = binnedImage.bits();
        const int rowStride = width * 3;
        frame.spectrum.resize(width * 3);
        frame.spectrum.fill(0.0);
        QVector<double> sumsR(width, 0.0), sumsG(width, 0.0), sumsB(width, 0.0);
        for (int y = startRow; y <= endRow; y++) {
            const uchar *row = srcData + y * rowStride;
            for (int x = 0; x < width; x++) {
                int idx = x * 3;
                sumsR[x] += row[idx];
                sumsG[x] += row[idx + 1];
                sumsB[x] += row[idx + 2];
            }
        }
        double maxVal = 0.0;
        for (int x = 0; x < width; x++) {
            frame.spectrum[x * 3 + 0] = sumsR[x];
            frame.spectrum[x * 3 + 1] = sumsG[x];
            frame.spectrum[x * 3 + 2] = sumsB[x];
            double vals[3] = { sumsR[x], sumsG[x], sumsB[x] };
            for (int c = 0; c < 3; c++) {
                if (vals[c] > maxVal) maxVal = vals[c];
            }
        }
        if (maxVal > 0.0 && maxVal <= 255.0) {
            for (int x = 0; x < width; x++) {
                dstData[x * 3] = static_cast<uchar>(sumsR[x]);
                dstData[x * 3 + 1] = static_cast<uchar>(sumsG[x]);
                dstData[x * 3 + 2] = static_cast<uchar>(sumsB[x]);
            }
        } else {
            for (int x = 0; x < width; x++) {
                dstData[x * 3] = static_cast<uchar>(sumsR[x] / maxVal * 255.0);
                dstData[x * 3 + 1] = static_cast<uchar>(sumsG[x] / maxVal * 255.0);
                dstData[x * 3 + 2] = static_cast<uchar>(sumsB[x] / maxVal * 255.0);
            }
        }
    }

    frame.image = binnedImage;
}

} // namespace PostProcess
