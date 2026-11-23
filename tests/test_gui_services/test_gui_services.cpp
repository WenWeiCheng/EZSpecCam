#include "test_gui_services.h"

#include <QDateTime>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTextStream>

#include "FileNaming.h"
#include "HistogramBinner.h"
#include "HistoryCsvExporter.h"
#include "ImageStatistics.h"

namespace
{

QImage makeGrayscale8(const QVector<int> &values, int width)
{
    const int height = values.size() / width;
    QImage image(width, height, QImage::Format_Grayscale8);
    for (int y = 0; y < height; ++y) {
        uchar *line = image.scanLine(y);
        for (int x = 0; x < width; ++x) {
            line[x] = static_cast<uchar>(values.at(y * width + x));
        }
    }
    return image;
}

} // namespace

void TestGuiServices::test_imageStatisticsFromGrayscale8()
{
    const QImage image = makeGrayscale8({0, 1, 2, 3}, 2);
    const ImageStatistics stats = ImageStatistics::computeImage(image);

    QCOMPARE(stats.minimum, 0.0);
    QCOMPARE(stats.maximum, 3.0);
    QCOMPARE(stats.average, 1.5);
    QCOMPARE(stats.pixelCount, quint64(4));
    QCOMPARE(stats.dataType, QStringLiteral("8-bit"));
    QVERIFY(qAbs(stats.stdDeviation - 1.118033988749895) < 1e-12);
}

void TestGuiServices::test_imageStatisticsFromSpectrum()
{
    const ImageStatistics stats = ImageStatistics::computeSpectrum({-1.0, 1.0});

    QCOMPARE(stats.minimum, -1.0);
    QCOMPARE(stats.maximum, 1.0);
    QCOMPARE(stats.average, 0.0);
    QCOMPARE(stats.pixelCount, quint64(2));
    QCOMPARE(stats.stdDeviation, 1.0);
    QCOMPARE(stats.dataType, QStringLiteral("Spectrum"));
}

void TestGuiServices::test_histogramBinningAndOverexposure()
{
    const QImage image = makeGrayscale8({0, 0, 10, 255}, 4);

    QCOMPARE(HistogramBinner::maxValueForImage(image), 255);

    const HistogramResult fine = HistogramBinner::compute(image, 255, 1, 10);
    QCOMPARE(fine.totalPixels, 4);
    QCOMPARE(fine.overexposedCount, 2); // 10 and 255
    QCOMPARE(fine.binCounts.size(), 256);
    QCOMPARE(fine.binCenters.size(), 256);
    QCOMPARE(fine.binCounts.at(0), 2);
    QCOMPARE(fine.binCounts.at(10), 1);
    QCOMPARE(fine.binCounts.at(255), 1);
    QCOMPARE(fine.binCenters.at(0), 0.5);

    const HistogramResult coarse = HistogramBinner::compute(image, 255, 256, 10);
    QCOMPARE(coarse.binCounts.size(), 1);
    QCOMPARE(coarse.binCounts.at(0), 4);
    QCOMPARE(coarse.overexposedCount, 2);
    QCOMPARE(coarse.binCenters.at(0), 128.0);
}

void TestGuiServices::test_historyCsvExport()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QVector<ParameterSample> samples{
        {1000, 1.25},
        {2000, -19.75},
    };

    const QString path = dir.filePath(QStringLiteral("history.csv"));
    QString error;
    QVERIFY2(HistoryCsvExporter::write(path, samples, 1000, &error), qPrintable(error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'),
                                                                     Qt::SkipEmptyParts);
    QCOMPARE(lines.size(), 3);
    QCOMPARE(lines.at(0), QStringLiteral("elapsed_s,timestamp,value"));
    QVERIFY(lines.at(1).startsWith(QStringLiteral("0.0,")));
    QVERIFY(lines.at(1).endsWith(QStringLiteral(",1.25")));
    QVERIFY(lines.at(2).startsWith(QStringLiteral("1.0,")));
    QVERIFY(lines.at(2).endsWith(QStringLiteral(",-19.75")));

    error.clear();
    QVERIFY(!HistoryCsvExporter::write(dir.filePath(QStringLiteral("empty.csv")),
                                       {}, 0, &error));
    QVERIFY(!error.isEmpty());
}

void TestGuiServices::test_fileNaming()
{
    const QDateTime timestamp(QDate(2026, 1, 2), QTime(3, 4, 5, 678), Qt::UTC);

    QCOMPARE(FileNaming::frameFileName(QStringLiteral("pre"), QStringLiteral("suf"),
                                       QStringLiteral("TIFF"), timestamp),
             QStringLiteral("pre_img_20260102_030405_678_suf.tiff"));
    QCOMPARE(FileNaming::frameFileName(QString(), QString(), QStringLiteral("CSV"), timestamp),
             QStringLiteral("img_20260102_030405_678.csv"));

    QCOMPARE(FileNaming::referenceStem(Calibration::ReferenceKind::LongDark),
             QStringLiteral("long_dark"));

    Calibration::ReferenceFrame reference;
    reference.exposure = 1.5;
    reference.frameCount = 3;
    QCOMPARE(FileNaming::referenceFileName(QStringLiteral("dark"), reference, timestamp),
             QStringLiteral("dark_1p5s_avg3_20260102_030405"));
}

QTEST_GUILESS_MAIN(TestGuiServices)
