#include <QCoreApplication>
#include <QObject>
#include <QTest>
#include <QImage>
#include <QElapsedTimer>
#include <QDebug>
#include <QVector>
#include <cmath>

#include "CameraTypes.h"
#include "PostProcess.h"

class TestPostProcessPerf : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {}
    void cleanupTestCase() {}

    void init() {}
    void cleanup() {}

    void test_verticalbinning_640x480_Grayscale16()
    {
        benchmarkVerticalBinning("PostProcess::verticalBinning 640x480 Grayscale16",
                                 640, 480, QImage::Format_Grayscale16, 1, 480);
    }

    void test_verticalbinning_1280x720_Grayscale16()
    {
        benchmarkVerticalBinning("PostProcess::verticalBinning 1280x720 Grayscale16",
                                 1280, 720, QImage::Format_Grayscale16, 1, 720);
    }

    void test_verticalbinning_1920x1080_Grayscale16()
    {
        benchmarkVerticalBinning("PostProcess::verticalBinning 1920x1080 Grayscale16",
                                 1920, 1080, QImage::Format_Grayscale16, 1, 1080);
    }

    void test_verticalbinning_6000x4000_Grayscale16()
    {
        benchmarkVerticalBinning("PostProcess::verticalBinning 6000x4000 Grayscale16",
                                 6000, 4000, QImage::Format_Grayscale16, 1, 4000);
    }

    void test_despike_6000_points()
    {
        benchmarkDespike("PostProcess::despike 6000 points", 6000);
    }

    void test_despike_2000_points()
    {
        benchmarkDespike("PostProcess::despike 2000 points", 2000);
    }

    void printStats(const QString &name, const QList<qint64> &timings)
    {
        if (timings.isEmpty()) return;

        qint64 sum = 0;
        qint64 min = timings[0];
        qint64 max = timings[0];
        for (qint64 t : timings) {
            sum += t;
            min = qMin(min, t);
            max = qMax(max, t);
        }
        double avg = sum / (double)timings.size();

        qDebug() << "==========================================";
        qDebug() << "Results for:" << name;
        qDebug() << "  Runs:" << timings.size();
        qDebug() << "  Average:" << avg << "ms";
        qDebug() << "  Min:" << min << "ms";
        qDebug() << "  Max:" << max << "ms";
        qDebug() << "  All times:" << timings;
    }

    void benchmarkDespike(const QString &name, int size)
    {
        // 内容固定：一条宽谱带 + 两个尖峰，保证每轮走的是同一条路径
        QVector<double> spectrum(size);
        unsigned state = 12345u;
        for (int i = 0; i < size; ++i) {
            state = state * 1664525u + 1013904223u;
            const double unit = static_cast<double>(state >> 8) / 16777216.0;
            spectrum[i] = 1000.0 + (unit - 0.5) * 40.0;
        }
        const double width = qMax(1.0, size / 40.0);
        for (int i = 0; i < size; ++i) {
            const double t = (i - size / 2.0) / width;
            spectrum[i] += 300.0 * std::exp(-0.5 * t * t);
        }
        spectrum[size / 6] += 5000.0;
        spectrum[size * 2 / 3] += 5000.0;

        // 单次调用在 Release 下不到 2 ms，逐次计时会被时钟精度吃掉，这里按微秒记
        const int runs = 20;
        QList<qint64> timingsUs;
        PostProcess::DespikeParams params;

        for (int i = 0; i < runs; ++i) {
            QElapsedTimer timer;
            timer.start();
            const PostProcess::DespikeResult result = PostProcess::despike(spectrum, params);
            timingsUs.append(timer.nsecsElapsed() / 1000);

            QCOMPARE(result.markedCount, 2);
            QVERIFY(result.marked[size / 6]);
            QVERIFY(result.marked[size * 2 / 3]);
        }

        qint64 sum = 0;
        qint64 min = timingsUs[0];
        qint64 max = timingsUs[0];
        for (const qint64 t : timingsUs) {
            sum += t;
            min = qMin(min, t);
            max = qMax(max, t);
        }

        qDebug() << "==========================================";
        qDebug() << "Results for:" << name;
        qDebug() << "  Runs:" << timingsUs.size();
        qDebug() << "  Average:" << (sum / static_cast<double>(timingsUs.size())) << "us";
        qDebug() << "  Min:" << min << "us";
        qDebug() << "  Max:" << max << "us";
    }

    void benchmarkVerticalBinning(const QString &name,
                                   int width, int height, QImage::Format format,
                                   int startRow, int endRow)
    {
        const int runs = 10;
        QList<qint64> timings;

        for (int i = 0; i < runs; ++i) {
            QImage image(width, height, format);
            if (format == QImage::Format_Grayscale16) {
                image.fill(0);
                for (int y = 0; y < height; ++y) {
                    quint16 *line = reinterpret_cast<quint16 *>(image.scanLine(y));
                    for (int x = 0; x < width; ++x) {
                        line[x] = static_cast<quint16>((x * 65535) / width);
                    }
                }
            } else if (format == QImage::Format_Grayscale8) {
                image.fill(0);
                for (int y = 0; y < height; ++y) {
                    uchar *line = image.scanLine(y);
                    for (int x = 0; x < width; ++x) {
                        line[x] = static_cast<uchar>((x * 255) / width);
                    }
                }
            } else if (format == QImage::Format_RGB888) {
                image.fill(0);
                for (int y = 0; y < height; ++y) {
                    uchar *line = image.scanLine(y);
                    for (int x = 0; x < width; ++x) {
                        int idx = x * 3;
                        line[idx] = static_cast<uchar>((x * 255) / width);
                        line[idx + 1] = static_cast<uchar>((x * 255) / width);
                        line[idx + 2] = static_cast<uchar>((x * 255) / width);
                    }
                }
            }

            ImageData frame;
            frame.image = image;
            frame.timestamp = QDateTime::currentMSecsSinceEpoch();

            QElapsedTimer timer;
            timer.start();
            PostProcess::verticalBinning(frame, startRow, endRow);
            timings.append(timer.elapsed());

            QVERIFY2(!frame.image.isNull(), "Output image should not be null");
            QVERIFY2(frame.image.height() == 1, "Output image should be 1 row high");
            QVERIFY2(frame.image.width() == width, "Output image width should match input width");
            QVERIFY2(!frame.spectrum.isEmpty(), "Spectrum should not be empty after binning");
        }

        printStats(name, timings);
    }
};

QTEST_MAIN(TestPostProcessPerf)
#include "test_postprocess_perf.moc"
