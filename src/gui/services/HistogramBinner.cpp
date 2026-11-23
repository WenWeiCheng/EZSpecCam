#include "HistogramBinner.h"

#include <QtGlobal>

namespace
{

constexpr int kMaxValue16Bit = 65535;

} // namespace

namespace HistogramBinner
{

int maxValueForImage(const QImage &image)
{
    return image.format() == QImage::Format_Grayscale16 ? kMaxValue16Bit : 255;
}

HistogramResult compute(const QImage &image, int maxValue, int valuesPerBin, int threshold)
{
    HistogramResult result;
    if (image.isNull() || maxValue < 0) {
        return result;
    }
    valuesPerBin = qMax(1, valuesPerBin);

    const int width = image.width();
    const int height = image.height();
    result.totalPixels = width * height;

    // The value range is 0..maxValue inclusive, so there are maxValue+1 values.
    // Round the bin count up and spread the width back over the range: every
    // bin then covers the same number of grey levels and the last one is not a
    // short leftover with an axis range that overshoots the data.
    const int valueCount = maxValue + 1;
    const int binCount = qMax(1, (valueCount + valuesPerBin - 1) / valuesPerBin);
    const double binWidth = static_cast<double>(valueCount) / binCount;

    QVector<int> counts(binCount, 0);

    if (image.format() == QImage::Format_Grayscale16) {
        for (int y = 0; y < height; ++y) {
            const quint16 *line = reinterpret_cast<const quint16 *>(image.constScanLine(y));
            for (int x = 0; x < width; ++x) {
                const int value = line[x];
                const int bin = qBound(0, static_cast<int>(value / binWidth), binCount - 1);
                ++counts[bin];
                if (value >= threshold) {
                    ++result.overexposedCount;
                }
            }
        }
    } else {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int value = qGray(image.pixel(x, y));
                const int bin = qBound(0, static_cast<int>(value / binWidth), binCount - 1);
                ++counts[bin];
                if (value >= threshold) {
                    ++result.overexposedCount;
                }
            }
        }
    }

    result.binCenters.reserve(binCount);
    result.binCounts.reserve(binCount);
    for (int i = 0; i < binCount; ++i) {
        result.binCenters.append((i + 0.5) * binWidth);
        result.binCounts.append(counts.at(i));
    }

    return result;
}

} // namespace HistogramBinner
