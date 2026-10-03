#include <QCoreApplication>
#include <QImage>
#include <QTest>

#include "HistogramViewWidget.h"
#include "HistogramWindow.h"
#include "qcustomplot.h"

static QCustomPlot *plotOf(HistogramViewWidget *widget)
{
    return widget->findChild<QCustomPlot *>();
}

static QCPBars *barsOf(HistogramViewWidget *widget)
{
    for (int i = 0; i < plotOf(widget)->plottableCount(); ++i) {
        if (auto *bars = dynamic_cast<QCPBars *>(plotOf(widget)->plottable(i))) {
            return bars;
        }
    }
    return nullptr;
}

// 8 位灰度图，width*height 个像素，值全为 value
static QImage flatImage(int size, int value)
{
    QImage image(size, size, QImage::Format_Grayscale8);
    image.fill(static_cast<uchar>(value));
    return image;
}

// 16 位灰度图，width*height 个像素，值全为 value。
// 每行都得整行填：只写 scanLine 的第一个像素的话，只有每行的第一个像素
// 拿到这个值，统计出来的箱会比预期少一大截
static QImage flatImage16(int size, int value)
{
    QImage image(size, size, QImage::Format_Grayscale16);
    for (int y = 0; y < size; ++y) {
        quint16 *line = reinterpret_cast<quint16 *>(image.scanLine(y));
        for (int x = 0; x < size; ++x) {
            line[x] = static_cast<quint16>(value);
        }
    }
    return image;
}

static void showLayout(QWidget *widget)
{
    widget->show();
    QCoreApplication::processEvents();
}

// 数一数画面上有多少个该颜色的像素。QCustomPlot 没有公开画刷的读接口，
// 想验证「柱子真的画出来了」只能落到像素上
static int countColor(QWidget *widget, const QColor &color)
{
    QImage image(widget->size(), QImage::Format_RGB32);
    image.fill(Qt::black);
    widget->render(&image);

    int n = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if ((image.pixel(x, y) & 0x00FFFFFF) == (color.rgb() & 0x00FFFFFF)) {
                ++n;
            }
        }
    }
    return n;
}

class TestHistogram : public QObject
{
    Q_OBJECT

private slots:
    void init() {}
    void cleanup() {}

    void test_uniform_image_lands_in_one_bin()
    {
        HistogramWindow window;
        window.setImage(flatImage(16, 100));

        QCOMPARE(window.totalPixelCount(), 256);
        QCOMPARE(window.binCounts().size(), 256);

        int nonEmpty = 0;
        int total = 0;
        for (int c : window.binCounts()) {
            total += c;
            if (c > 0) {
                ++nonEmpty;
            }
        }
        QCOMPARE(total, 256);
        QCOMPARE(nonEmpty, 1);

        // 值 100 在 0..255 上分 256 箱，箱宽正好 1，落在第 100 箱
        QCOMPARE(window.binCounts().at(100), 256);
        QVERIFY(qAbs(window.binCenters().at(100) - 100.5) < 1e-9);
    }

    void test_overexposure_threshold_is_customisable()
    {
        HistogramWindow window;
        // 0, 100, 250 各一个像素，外加若干 10
        QImage image(4, 4, QImage::Format_Grayscale8);
        image.fill(10);
        image.setPixel(0, 0, qRgb(0, 0, 0));
        image.setPixel(1, 0, qRgb(100, 100, 100));
        image.setPixel(2, 0, qRgb(250, 250, 250));
        window.setImage(image);

        QCOMPARE(window.totalPixelCount(), 16);
        QCOMPARE(window.maxValue(), 255);

        // 默认阈值是量程顶端（255），只有 250 那个不算，255 才算过曝
        QCOMPARE(window.overexposureThreshold(), 255);
        QCOMPARE(window.overexposedPixelCount(), 0);

        // 阈值改成 251：250 还是不算
        window.setOverexposureThreshold(251);
        QCOMPARE(window.overexposedPixelCount(), 0);

        // 阈值改成 250：只有 250 那个算
        window.setOverexposureThreshold(250);
        QCOMPARE(window.overexposedPixelCount(), 1);

        // 阈值改成 100：250 和 100 都算
        window.setOverexposureThreshold(100);
        QCOMPARE(window.overexposedPixelCount(), 2);

        // 阈值改成 0：整幅图都算
        window.setOverexposureThreshold(0);
        QCOMPARE(window.overexposedPixelCount(), 16);

        // 阈值回到 255，判定跟着回去
        window.setOverexposureThreshold(255);
        QCOMPARE(window.overexposedPixelCount(), 0);
    }

    void test_threshold_range_follows_image_depth()
    {
        HistogramWindow window;

        // 8 位图配 65535 的阈值永远命中不了，所以量程要跟着格式收。
        // 默认阈值落在量程顶端，8 位下就是 255：整幅 255 的图全算过曝
        window.setImage(flatImage(8, 255));
        QCOMPARE(window.maxValue(), 255);
        QCOMPARE(window.overexposureThreshold(), 255);
        QCOMPARE(window.overexposedPixelCount(), 64);

        // 换 16 位图，量程放开，阈值保持
        window.setImage(flatImage16(8, 20000));
        QCOMPARE(window.maxValue(), 65535);
        QCOMPARE(window.overexposureThreshold(), 255);
        QCOMPARE(window.overexposedPixelCount(), 64);

        // 换回 8 位图，用户设过的阈值要被夹回量程内，而不是留在 65535
        window.setOverexposureThreshold(60000);
        window.setImage(flatImage(8, 10));
        QCOMPARE(window.overexposureThreshold(), 255);
        QCOMPARE(window.overexposedPixelCount(), 0);
    }

    void test_16bit_image_uses_16bit_bins()
    {
        HistogramWindow window;
        window.setImage(flatImage16(4, 50000));

        QCOMPARE(window.maxValue(), 65535);
        QCOMPARE(window.totalPixelCount(), 16);
        // 箱宽 256，50000 落在 195 号箱（50000/256 = 195.3）
        QCOMPARE(window.binCounts().at(195), 16);
        QCOMPARE(window.binCounts().at(0), 0);
    }

    void test_log_axis_keeps_bar_geometry_finite()
    {
        // 核心用例：QCPBars::getBarRect 拿 coordToPixel(base + value) 算矩形，
        // 对数轴上 base 若是 0，log10(0) = -inf，柱子的几何就废了。
        // 直方图里大量箱子计数为 0，这个坑必踩
        HistogramViewWidget widget;
        widget.resize(400, 300);

        // 一半落在 0 值箱（对数轴上画不出来），一半落在 100
        QVector<double> bins;
        QVector<double> counts;
        for (int i = 0; i < 8; ++i) {
            bins.append(i + 0.5);
            counts.append((i == 0) ? 500 : 10);
        }
        widget.setHistogram(bins, counts);
        showLayout(&widget);

        auto *plot = plotOf(&widget);
        QVERIFY(plot);
        auto *bars = barsOf(&widget);
        QVERIFY(bars);

        // 线性轴下柱子底是 0
        QCOMPARE(bars->baseValue(), 0.0);

        widget.setLogScale(true);

        // 柱子底必须为正
        QVERIFY2(bars->baseValue() > 0.0,
                 qPrintable(QStringLiteral("对数轴下柱子底还是 %1").arg(bars->baseValue())));

        // 轴的下界必须为正，否则对数刻度算不出来
        QVERIFY2(plot->yAxis->range().lower > 0.0,
                 qPrintable(QStringLiteral("对数轴下界不是正数：%1").arg(plot->yAxis->range().lower)));
        QCOMPARE(plot->yAxis->scaleType(), QCPAxis::stLogarithmic);

        // getBarRect 是 protected，但它算矩形用的就是这个转换：
        // valuePixel = valueAxis->coordToPixel(baseValue + value)。
        // 操作数一旦非正，log10 给出 -inf，整根柱子的几何就废了
        for (int i = 0; i < counts.size(); ++i) {
            const double pixel = plot->yAxis->coordToPixel(bars->baseValue() + counts.at(i));
            QVERIFY2(qIsFinite(pixel),
                     qPrintable(QStringLiteral("第 %1 根柱子的纵向坐标是 %2（base=%3 value=%4）")
                                    .arg(i).arg(pixel)
                                    .arg(bars->baseValue()).arg(counts.at(i))));
        }

        // 再落到画面上：对数轴下柱子必须真的画出来，而不是被静默丢掉
        bars->setBrush(QBrush(Qt::magenta));
        plot->replot();
        QVERIFY2(countColor(&widget, Qt::magenta) > 0, "对数轴下柱子一个像素都没画出来");
    }

    void test_log_axis_base_moves_back_to_zero()
    {
        HistogramViewWidget widget;
        widget.resize(400, 300);

        QVector<double> bins;
        QVector<double> counts;
        for (int i = 0; i < 8; ++i) {
            bins.append(i + 0.5);
            counts.append(10);
        }
        widget.setHistogram(bins, counts);
        showLayout(&widget);

        QVERIFY(barsOf(&widget)->baseValue() == 0.0);

        widget.setLogScale(true);
        QVERIFY(barsOf(&widget)->baseValue() > 0.0);

        // 切回线性轴，柱子底要跟着回到 0，否则直方图会从 1 往上画，看着缺了一截
        widget.setLogScale(false);
        QCOMPARE(barsOf(&widget)->baseValue(), 0.0);
        QCOMPARE(plotOf(&widget)->yAxis->scaleType(), QCPAxis::stLinear);
        QVERIFY(plotOf(&widget)->yAxis->range().lower == 0.0);
    }

    void test_histogram_follows_new_frame()
    {
        // live 模式下窗口开着就该跟着新帧重算
        HistogramWindow window;
        window.setImage(flatImage(8, 10));
        QCOMPARE(window.binCounts().at(10), 64);
        QCOMPARE(window.overexposedPixelCount(), 0);

        window.setImage(flatImage(8, 255));
        QCOMPARE(window.binCounts().at(10), 0);
        QCOMPARE(window.binCounts().at(255), 64);
        QCOMPARE(window.overexposedPixelCount(), 64);
    }

    void test_bars_fill_the_plot_area()
    {
        // 箱宽走数据坐标，不设置的话 256 根柱子会按绘图区比例铺开糊成一块
        HistogramViewWidget widget;
        widget.resize(400, 300);

        QVector<double> bins;
        QVector<double> counts;
        for (int i = 0; i < 256; ++i) {
            bins.append(i + 0.5);
            counts.append(1);
        }
        widget.setHistogram(bins, counts);
        showLayout(&widget);

        auto *bars = barsOf(&widget);
        QVERIFY(bars);
        QCOMPARE(bars->widthType(), QCPBars::wtPlotCoords);
        QVERIFY(qAbs(bars->width() - 1.0) < 1e-9);

        // 每根柱子都要比一个像素宽，密到看不见说明宽度算错了。
        // 换算到屏幕上量：QCPBars 用的就是这个 coordToPixel
        auto *plot = plotOf(&widget);
        QVERIFY(plot);
        const double left = plot->xAxis->coordToPixel(0.5 - 0.5);
        const double right = plot->xAxis->coordToPixel(0.5 + 0.5);
        QVERIFY2(right - left > 1.0,
                 qPrintable(QStringLiteral("单根柱子只有 %1 像素宽").arg(right - left)));

        // 首尾两根柱子都要落在绘图区里，不能被边沿切掉
        const QRect area = plot->axisRect()->rect();
        QVERIFY(plot->xAxis->coordToPixel(0.0) >= area.left() - 1.0);
        QVERIFY(plot->xAxis->coordToPixel(256.0) <= area.right() + 1.0);    }
};

QTEST_MAIN(TestHistogram)
#include "test_histogram.moc"
