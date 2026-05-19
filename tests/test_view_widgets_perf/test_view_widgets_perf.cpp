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

    void init() {}
    void cleanup() {}

    void test_imageviewwidget_6000x6000_with_downsampling()
    {
        const int runs = 10;
        QList<qint64> timings;

        for (int i = 0; i < runs; ++i) {
            ImageViewWidget widget;
            widget.resize(800, 600);
            QApplication::processEvents();

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
            timings.append(timer.elapsed());
        }

        printStats("ImageViewWidget 6000x6000 DS ON", timings);
    }

    void test_imageviewwidget_6000x6000_without_downsampling()
    {
        const int runs = 10;
        QList<qint64> timings;

        for (int i = 0; i < runs; ++i) {
            ImageViewWidget widget;
            widget.resize(800, 600);
            QApplication::processEvents();
            widget.setDownsamplingEnabled(false);

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
            timings.append(timer.elapsed());
        }

        printStats("ImageViewWidget 6000x6000 DS OFF", timings);
    }

    void test_imageviewwidget_viewport_sizes()
    {
        QList<QSize> viewportSizes = {
            QSize(320, 240),
            QSize(800, 600),
            QSize(1920, 1080)
        };

        for (const QSize &size : viewportSizes) {
            const int runs = 10;
            QList<qint64> timings;

            for (int i = 0; i < runs; ++i) {
                ImageViewWidget widget;
                widget.resize(size);
                QApplication::processEvents();

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
                timings.append(timer.elapsed());
            }

            printStats(QString("ImageViewWidget 6000x6000 %1x%2").arg(size.width()).arg(size.height()), timings);
        }
    }

    void test_spectrumviewwidget_6000x1()
    {
        const int runs = 10;
        QList<qint64> timings;

        for (int i = 0; i < runs; ++i) {
            SpectrumViewWidget widget;
            widget.resize(800, 600);

            QImage image(6000, 1, QImage::Format_Grayscale16);
            image.fill(0);
            quint16 *line = reinterpret_cast<quint16 *>(image.scanLine(0));
            for (int x = 0; x < 6000; ++x) {
                line[x] = static_cast<quint16>((x * 65535) / 6000);
            }

            QElapsedTimer timer;
            timer.start();
            widget.setFromImage(image);
            timings.append(timer.elapsed());

            QVERIFY2(widget.hasData(), "Widget should have data after setFromImage");
            QVERIFY2(widget.dataWidth() == 6000, "Data width should be 6000");
        }

        printStats("SpectrumViewWidget 6000x1", timings);
    }

    void test_spectrumviewwidget_6000_points()
    {
        const int runs = 10;
        QList<qint64> timings;

        for (int i = 0; i < runs; ++i) {
            SpectrumViewWidget widget;
            widget.resize(800, 600);

            QVector<double> xData(6000);
            QVector<double> yData(6000);
            for (int j = 0; j < 6000; ++j) {
                xData[j] = j;
                yData[j] = (j * 100.0) / 6000;
            }

            QElapsedTimer timer;
            timer.start();
            widget.setData(xData, yData);
            timings.append(timer.elapsed());

            QVERIFY2(widget.hasData(), "Widget should have data after setData");
            QVERIFY2(widget.dataWidth() == 6000, "Data width should be 6000");
        }

        printStats("SpectrumViewWidget 6000 points", timings);
    }

    void test_spectrumviewwidget_setSpectrumData_640()
    {
        const int runs = 10;
        QList<qint64> timings;

        for (int i = 0; i < runs; ++i) {
            SpectrumViewWidget widget;
            widget.resize(800, 600);

            QVector<quint64> spectrum(640);
            for (int j = 0; j < 640; ++j) {
                spectrum[j] = (j * 100) ;
            }

            QElapsedTimer timer;
            timer.start();
            widget.setSpectrumData(spectrum);
            timings.append(timer.elapsed());

            QVERIFY2(widget.hasData(), "Widget should have data after setSpectrumData");
            QVERIFY2(widget.dataWidth() == 640, "Data width should be 640");
        }

        printStats("SpectrumViewWidget::setSpectrumData 640 points", timings);
    }

    void test_spectrumviewwidget_setSpectrumData_1920()
    {
        const int runs = 10;
        QList<qint64> timings;

        for (int i = 0; i < runs; ++i) {
            SpectrumViewWidget widget;
            widget.resize(800, 600);

            QVector<quint64> spectrum(1920);
            for (int j = 0; j < 1920; ++j) {
                spectrum[j] = (j * 100);
            }

            QElapsedTimer timer;
            timer.start();
            widget.setSpectrumData(spectrum);
            timings.append(timer.elapsed());

            QVERIFY2(widget.hasData(), "Widget should have data after setSpectrumData");
            QVERIFY2(widget.dataWidth() == 1920, "Data width should be 1920");
        }

        printStats("SpectrumViewWidget::setSpectrumData 1920 points", timings);
    }

    void test_spectrumviewwidget_setSpectrumData_6000()
    {
        const int runs = 10;
        QList<qint64> timings;

        for (int i = 0; i < runs; ++i) {
            SpectrumViewWidget widget;
            widget.resize(800, 600);

            QVector<quint64> spectrum(6000);
            for (int j = 0; j < 6000; ++j) {
                spectrum[j] = (j * 100);
            }

            QElapsedTimer timer;
            timer.start();
            widget.setSpectrumData(spectrum);
            timings.append(timer.elapsed());

            QVERIFY2(widget.hasData(), "Widget should have data after setSpectrumData");
            QVERIFY2(widget.dataWidth() == 6000, "Data width should be 6000");
        }

        printStats("SpectrumViewWidget::setSpectrumData 6000 points", timings);
    }

    void test_imageviewwidget_repeated_sets()
    {
        const int runs = 10;
        ImageViewWidget widget;
        widget.resize(800, 600);
        QApplication::processEvents();

        QImage image(6000, 6000, QImage::Format_Grayscale16);
        image.fill(0);
        for (int y = 0; y < 6000; ++y) {
            quint16 *line = reinterpret_cast<quint16 *>(image.scanLine(y));
            for (int x = 0; x < 6000; ++x) {
                line[x] = static_cast<quint16>((x * 65535) / 6000);
            }
        }

        QList<qint64> timings;
        for (int i = 0; i < runs; ++i) {
            QElapsedTimer timer;
            timer.start();
            widget.setImage(image);
            timings.append(timer.elapsed());
        }

        printStats("ImageViewWidget repeated setImage", timings);
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
};

QTEST_MAIN(TestViewWidgetsPerf)
#include "test_view_widgets_perf.moc"