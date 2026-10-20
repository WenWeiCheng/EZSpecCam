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

    void test_despike_image_6000x4000_Grayscale16()
    {
        benchmarkImageDespike("PostProcess::despikeImage 6000x4000 Grayscale16 (full image)",
                              6000, 4000, QImage::Format_Grayscale16, 0, -1);
    }

    void test_despike_image_6000x4000_Grayscale8()
    {
        benchmarkImageDespike("PostProcess::despikeImage 6000x4000 Grayscale8 (full image)",
                              6000, 4000, QImage::Format_Grayscale8, 0, -1);
    }

    void test_despike_image_6000x4000_row_range_100()
    {
        // 典型光谱带：6000x4000 里只处理 100 行，其余行原样复制
        benchmarkImageDespike("PostProcess::despikeImage 6000x4000 Grayscale16 (100 rows)",
                              6000, 4000, QImage::Format_Grayscale16, 2000, 2099);
    }

    void test_despike_image_1920x1080_Grayscale16()
    {
        benchmarkImageDespike("PostProcess::despikeImage 1920x1080 Grayscale16 (full image)",
                              1920, 1080, QImage::Format_Grayscale16, 0, -1);
    }

    void test_despike_vbin_roi_6000x4000_100_rows()
    {
        // MainWindow 在 vbin + 部分行范围时走的就是这条路：只复制 ROI（带上下
        // 各一行 halo）去尖峰，然后只对 ROI 做垂直合并
        benchmarkDespikeRoi("despike + vbin ROI 6000x4000 (100 rows)", 6000, 4000, 100);
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

    // 图像去尖峰：噪声背景 + 按固定网格铺开的孤立尖峰。网格间距远大于 3×3。
    // 行范围以外的尖峰必须保持原样，范围内的必须全部摘掉，处理点数也是确定值
    void benchmarkImageDespike(const QString &name, int width, int height,
                               QImage::Format format, int firstRow, int lastRow)
    {
        constexpr int kSpikeSpacing = 337;
        constexpr int kSpikeMargin = 37;

        if (lastRow < 0) {
            lastRow = height - 1;
        }

        QImage image(width, height, format);
        unsigned state = 987654321u;
        auto nextNoise = [&state]() {
            state = state * 1664525u + 1013904223u;
            return static_cast<double>(state >> 8) / 16777216.0 - 0.5;
        };

        if (format == QImage::Format_Grayscale16) {
            for (int y = 0; y < height; ++y) {
                quint16 *line = reinterpret_cast<quint16 *>(image.scanLine(y));
                for (int x = 0; x < width; ++x) {
                    line[x] = static_cast<quint16>(1000.0 + nextNoise() * 20.0);
                }
            }
        } else {
            for (int y = 0; y < height; ++y) {
                uchar *line = image.scanLine(y);
                for (int x = 0; x < width; ++x) {
                    line[x] = static_cast<uchar>(100.0 + nextNoise() * 20.0);
                }
            }
        }

        int plantedInside = 0;
        for (int y = kSpikeMargin; y + kSpikeMargin < height; y += kSpikeSpacing) {
            for (int x = kSpikeMargin; x + kSpikeMargin < width; x += kSpikeSpacing) {
                if (format == QImage::Format_Grayscale16) {
                    quint16 *line = reinterpret_cast<quint16 *>(image.scanLine(y));
                    line[x] = static_cast<quint16>(line[x] + 20000);
                } else {
                    image.scanLine(y)[x] = 250;
                }
                if (y >= firstRow && y <= lastRow && y > 0 && y < height - 1) {
                    ++plantedInside;
                }
            }
        }

        const int runs = 5;
        QList<qint64> timingsMs;
        for (int i = 0; i < runs; ++i) {
            PostProcess::ImageDespikeResult result;
            QElapsedTimer timer;
            timer.start();
            const QImage output = PostProcess::despikeImage(image,
                                                            PostProcess::DespikeParams(),
                                                            &result,
                                                            firstRow, lastRow);
            timingsMs.append(timer.nsecsElapsed() / 1000000);

            QVERIFY(!output.isNull());
            QCOMPARE(output.size(), image.size());
            QCOMPARE(result.markedCount, plantedInside);
        }

        qint64 sum = 0;
        qint64 min = timingsMs[0];
        qint64 max = timingsMs[0];
        for (const qint64 t : timingsMs) {
            sum += t;
            min = qMin(min, t);
            max = qMax(max, t);
        }
        const double avg = sum / static_cast<double>(timingsMs.size());

        // 用 qInfo 而不是 qDebug：Release 构建定义了 QT_NO_DEBUG_OUTPUT，
        // 性能数字写在 qDebug 里会被编译掉，基准测试就白跑了
        qInfo() << "==========================================";
        qInfo() << "Results for:" << name;
        qInfo() << "  Runs:" << timingsMs.size();
        qInfo() << "  Average:" << avg << "ms";
        qInfo() << "  Min:" << min << "ms";
        qInfo() << "  Max:" << max << "ms";
        qInfo() << "  FPS (avg):" << (avg > 0.0 ? 1000.0 / avg : 0.0);
        qInfo() << "  FPS (best):" << (min > 0 ? 1000.0 / static_cast<double>(min) : 0.0);
    }

    void benchmarkDespikeRoi(const QString &name, int width, int height, int rows)
    {
        const int firstRow = 2000;
        const int lastRow = firstRow + rows - 1;
        const int haloFirst = qMax(0, firstRow - 1);
        const int haloLast = qMin(height - 1, lastRow + 1);
        const int roiFirstInRoi = firstRow - haloFirst;
        const int roiLastInRoi = lastRow - haloFirst;

        QImage image(width, height, QImage::Format_Grayscale16);
        unsigned state = 13579u;
        for (int y = 0; y < height; ++y) {
            quint16 *line = reinterpret_cast<quint16 *>(image.scanLine(y));
            for (int x = 0; x < width; ++x) {
                state = state * 1664525u + 1013904223u;
                const double unit = static_cast<double>(state >> 8) / 16777216.0;
                line[x] = static_cast<quint16>(1000.0 + (unit - 0.5) * 20.0);
            }
        }

        const int runs = 5;
        QList<qint64> timingsMs;
        qint64 copyUs = 0;
        qint64 despikeUs = 0;
        qint64 binUs = 0;
        for (int i = 0; i < runs; ++i) {
            PostProcess::ImageDespikeResult result;
            QElapsedTimer timer;
            QElapsedTimer total;
            total.start();

            timer.start();
            const QImage roi = image.copy(0, haloFirst, width, haloLast - haloFirst + 1);
            copyUs += timer.nsecsElapsed() / 1000;

            timer.restart();
            const QImage processed = PostProcess::despikeImage(roi, PostProcess::DespikeParams(),
                                                               &result,
                                                               roiFirstInRoi, roiLastInRoi);
            despikeUs += timer.nsecsElapsed() / 1000;

            timer.restart();
            ImageData frame;
            frame.image = processed;
            PostProcess::verticalBinning(frame, roiFirstInRoi + 1, roiLastInRoi + 1);
            binUs += timer.nsecsElapsed() / 1000;

            timingsMs.append(total.nsecsElapsed() / 1000000);

            QCOMPARE(result.sampleCount, (width - 2) * rows);
            QCOMPARE(frame.image.height(), 1);
            QCOMPARE(frame.spectrum.size(), width);
        }

        qint64 sum = 0;
        qint64 min = timingsMs[0];
        qint64 max = timingsMs[0];
        for (const qint64 t : timingsMs) {
            sum += t;
            min = qMin(min, t);
            max = qMax(max, t);
        }
        const double avg = sum / static_cast<double>(timingsMs.size());

        qInfo() << "==========================================";
        qInfo() << "Results for:" << name;
        qInfo() << "  Runs:" << timingsMs.size();
        qInfo() << "  Average:" << avg << "ms";
        qInfo() << "  Min:" << min << "ms";
        qInfo() << "  Max:" << max << "ms";
        qInfo() << "  FPS (avg):" << (avg > 0.0 ? 1000.0 / avg : 0.0);
        qInfo() << "  FPS (best):" << (min > 0 ? 1000.0 / static_cast<double>(min) : 0.0);
        qInfo() << "  ROI copy:" << (copyUs / 1000.0 / runs) << "ms"
                << " despike:" << (despikeUs / 1000.0 / runs) << "ms"
                << " vbin:" << (binUs / 1000.0 / runs) << "ms";
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
