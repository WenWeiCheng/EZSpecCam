#include <QCoreApplication>
#include <QObject>
#include <QTest>
#include <QImage>
#include <QElapsedTimer>
#include <QDebug>
#include <QVector>
#include "ImageViewWidget.h"
#include "SpectrumViewWidget.h"

class TestViewWidgetsPerf : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {}
    void cleanupTestCase() {}

    void init()
    {
    }

    void cleanup()
    {
    }

    void test_imageviewwidget_6000x6000_with_downsampling()
    {
        ImageViewWidget widget;
        widget.resize(800, 600);
        QTest::qWait(100);

        QImage image(6000, 6000, QImage::Format_Grayscale16);
        image.fill(0);

        for (int y = 0; y < 6000; ++y) {
            quint16 *line = reinterpret_cast<quint16 *>(image.scanLine(y));
            for (int x = 0; x < 6000; ++x) {
                line[x] = static_cast<quint16>((x * 65535) / 6000);
            }
        }

        QVERIFY(widget.isDownsamplingEnabled() == true);

        QElapsedTimer timer;
        timer.start();

        widget.setImage(image);
        QTest::qWait(100);

        qint64 elapsed = timer.elapsed();
        qDebug() << "ImageViewWidget 6000x6000 (downsampling ON, 800x600 viewport):" << elapsed << "ms";
    }

    void test_imageviewwidget_6000x6000_without_downsampling()
    {
        ImageViewWidget widget;
        widget.resize(800, 600);
        QTest::qWait(100);

        QImage image(6000, 6000, QImage::Format_Grayscale16);
        image.fill(0);

        for (int y = 0; y < 6000; ++y) {
            quint16 *line = reinterpret_cast<quint16 *>(image.scanLine(y));
            for (int x = 0; x < 6000; ++x) {
                line[x] = static_cast<quint16>((x * 65535) / 6000);
            }
        }

        widget.setDownsamplingEnabled(false);
        QVERIFY(widget.isDownsamplingEnabled() == false);

        QElapsedTimer timer;
        timer.start();

        widget.setImage(image);
        QTest::qWait(200);

        qint64 elapsed = timer.elapsed();
        qDebug() << "ImageViewWidget 6000x6000 (downsampling OFF, 800x600 viewport):" << elapsed << "ms";
    }

    void test_imageviewwidget_viewport_sizes()
    {
        QList<QSize> viewportSizes = {
            QSize(320, 240),
            QSize(800, 600),
            QSize(1920, 1080)
        };

        for (const QSize &size : viewportSizes) {
            ImageViewWidget widget;
            widget.resize(size);
            QTest::qWait(100);

            QImage image(6000, 6000, QImage::Format_Grayscale16);
            image.fill(0);

            for (int y = 0; y < 6000; ++y) {
                quint16 *line = reinterpret_cast<quint16 *>(image.scanLine(y));
                for (int x = 0; x < 6000; ++x) {
                    line[x] = static_cast<quint16>((x * 65535) / 6000);
                }
            }

            QElapsedTimer timer;
            timer.start();

            widget.setImage(image);
            QTest::qWait(100);

            qint64 elapsed = timer.elapsed();
            qDebug() << "ImageViewWidget 6000x6000 viewport" << size << ":" << elapsed << "ms";
        }
    }

    void test_spectrumviewwidget_6000x1()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);
        QTest::qWait(100);

        QImage image(6000, 1, QImage::Format_Grayscale16);
        image.fill(0);

        quint16 *line = reinterpret_cast<quint16 *>(image.scanLine(0));
        for (int x = 0; x < 6000; ++x) {
            line[x] = static_cast<quint16>((x * 65535) / 6000);
        }

        QElapsedTimer timer;
        timer.start();

        widget.setFromImage(image);
        QTest::qWait(100);

        qint64 elapsed = timer.elapsed();
        qDebug() << "SpectrumViewWidget 6000x1 (setFromImage):" << elapsed << "ms";
        QVERIFY2(widget.hasData(), "Widget should have data after setFromImage");
        QVERIFY2(widget.dataWidth() == 6000, "Data width should be 6000");
    }

    void test_spectrumviewwidget_6000_points()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);
        QTest::qWait(100);

        QVector<double> xData(6000);
        QVector<double> yData(6000);

        for (int i = 0; i < 6000; ++i) {
            xData[i] = i;
            yData[i] = (i * 100.0) / 6000;
        }

        QElapsedTimer timer;
        timer.start();

        widget.setData(xData, yData);
        QTest::qWait(100);

        qint64 elapsed = timer.elapsed();
        qDebug() << "SpectrumViewWidget 6000 points (setData):" << elapsed << "ms";
        QVERIFY2(widget.hasData(), "Widget should have data after setData");
        QVERIFY2(widget.dataWidth() == 6000, "Data width should be 6000");
    }

    void test_imageviewwidget_stress_repeated_sets()
    {
        ImageViewWidget widget;
        widget.resize(800, 600);
        QTest::qWait(100);

        QImage image(6000, 6000, QImage::Format_Grayscale16);
        image.fill(0);

        for (int y = 0; y < 6000; ++y) {
            quint16 *line = reinterpret_cast<quint16 *>(image.scanLine(y));
            for (int x = 0; x < 6000; ++x) {
                line[x] = static_cast<quint16>((x * 65535) / 6000);
            }
        }

        QList<qint64> timings;
        for (int i = 0; i < 5; ++i) {
            QElapsedTimer timer;
            timer.start();

            widget.setImage(image);
            QTest::qWait(100);

            timings.append(timer.elapsed());
        }

        qint64 avg = 0;
        for (qint64 t : timings) avg += t;
        avg /= timings.size();

        qDebug() << "ImageViewWidget repeated setImage (5 runs):";
        qDebug() << "  Individual times:" << timings;
        qDebug() << "  Average:" << avg << "ms";
    }
};

QTEST_MAIN(TestViewWidgetsPerf)
#include "test_view_widgets_perf.moc"