#include "ImageStatistics.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{

bool is16BitFormat(QImage::Format format)
{
    return format == QImage::Format_Grayscale16
        || format == QImage::Format_RGB16
        || format == QImage::Format_RGBA64;
}

} // namespace

ImageStatistics ImageStatistics::computeImage(const QImage &image)
{
    ImageStatistics stats;
    if (image.isNull()) {
        stats.dataType = QStringLiteral("None");
        return stats;
    }

    const uchar *bits = image.constBits();
    const int width = image.width();
    const int height = image.height();
    const int bytesPerLine = image.bytesPerLine();
    const QImage::Format format = image.format();

    double sum = 0.0;
    double sumSq = 0.0;
    quint64 count = 0;
    stats.minimum = std::numeric_limits<double>::max();
    stats.maximum = std::numeric_limits<double>::lowest();

    for (int y = 0; y < height; ++y) {
        const uchar *row = bits + y * bytesPerLine;
        for (int x = 0; x < width; ++x) {
            double value = 0.0;

            if (format == QImage::Format_Grayscale8
                || format == QImage::Format_Indexed8) {
                value = row[x];
            } else if (format == QImage::Format_Grayscale16) {
                const ushort *row16 = reinterpret_cast<const ushort *>(row);
                value = row16[x];
            } else if (format == QImage::Format_RGB16) {
                const ushort *row16 = reinterpret_cast<const ushort *>(row);
                value = row16[x];
            } else if (format == QImage::Format_RGB888) {
                value = 0.299 * row[x * 3] + 0.587 * row[x * 3 + 1]
                    + 0.114 * row[x * 3 + 2];
            } else if (format == QImage::Format_RGBA64) {
                // 16 bits per channel; QRgb (32-bit) would read the wrong stride.
                const ushort *row16 = reinterpret_cast<const ushort *>(row);
                value = 0.299 * row16[x * 4] + 0.587 * row16[x * 4 + 1]
                    + 0.114 * row16[x * 4 + 2];
            } else if (format == QImage::Format_RGB32
                       || format == QImage::Format_ARGB32) {
                const QRgb *pixel = reinterpret_cast<const QRgb *>(row + x * 4);
                value = 0.299 * qRed(*pixel) + 0.587 * qGreen(*pixel)
                    + 0.114 * qBlue(*pixel);
            } else {
                value = row[x];
            }

            stats.minimum = std::min(stats.minimum, value);
            stats.maximum = std::max(stats.maximum, value);
            sum += value;
            sumSq += value * value;
            ++count;
        }
    }

    if (count == 0) {
        stats.minimum = 0.0;
        stats.maximum = 0.0;
        stats.dataType = QStringLiteral("None");
        return stats;
    }

    stats.pixelCount = count;
    stats.average = sum / count;
    const double variance = std::max(0.0, sumSq / count - stats.average * stats.average);
    stats.stdDeviation = std::sqrt(variance);
    stats.dataType = is16BitFormat(format) ? QStringLiteral("16-bit")
                                           : QStringLiteral("8-bit");
    return stats;
}

ImageStatistics ImageStatistics::computeSpectrum(const QVector<double> &data)
{
    ImageStatistics stats;
    if (data.isEmpty()) {
        stats.dataType = QStringLiteral("None");
        return stats;
    }

    double sum = 0.0;
    double sumSq = 0.0;
    stats.minimum = std::numeric_limits<double>::max();
    stats.maximum = std::numeric_limits<double>::lowest();

    for (double value : data) {
        stats.minimum = std::min(stats.minimum, value);
        stats.maximum = std::max(stats.maximum, value);
        sum += value;
        sumSq += value * value;
    }

    stats.pixelCount = static_cast<quint64>(data.size());
    stats.average = sum / stats.pixelCount;
    const double variance = std::max(0.0, sumSq / stats.pixelCount
                                            - stats.average * stats.average);
    stats.stdDeviation = std::sqrt(variance);
    stats.dataType = QStringLiteral("Spectrum");
    return stats;
}
