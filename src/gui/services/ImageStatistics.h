#ifndef IMAGESTATISTICS_H
#define IMAGESTATISTICS_H

#include <QImage>
#include <QString>
#include <QVector>

//! Result of scanning one image or one spectrum.
//!
//! This is plain data: the computation lives here instead of inside
//! StatisticsDialog so it can be unit-tested without a QApplication.
struct ImageStatistics
{
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double stdDeviation = 0.0;
    quint64 pixelCount = 0;
    //! "8-bit", "16-bit", "Spectrum" or "None" for an empty input.
    QString dataType;

    static ImageStatistics computeImage(const QImage &image);
    static ImageStatistics computeSpectrum(const QVector<double> &data);
};

#endif // IMAGESTATISTICS_H
