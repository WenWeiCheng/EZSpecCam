#ifndef HISTOGRAMBINNER_H
#define HISTOGRAMBINNER_H

#include <QImage>
#include <QVector>

//! Histogram bins and the overexposure count for one image.
struct HistogramResult
{
    QVector<double> binCenters;
    QVector<double> binCounts;
    int overexposedCount = 0;
    int totalPixels = 0;
};

//! Pure binning helpers used by HistogramWindow.
namespace HistogramBinner
{

//! 65535 for Grayscale16, 255 for every other format the GUI displays.
int maxValueForImage(const QImage &image);

//! One pass over @p image: bin every pixel into @p valuesPerBin grey levels and
//! count pixels at or above @p threshold. @p maxValue is the inclusive upper
//! bound of the value range (255 or 65535).
HistogramResult compute(const QImage &image, int maxValue, int valuesPerBin, int threshold);

} // namespace HistogramBinner

#endif // HISTOGRAMBINNER_H
