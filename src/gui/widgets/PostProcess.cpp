#include "PostProcess.h"

#include <algorithm>
#include <omp.h>

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

} // namespace PostProcess
