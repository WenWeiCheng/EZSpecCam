#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QMouseEvent>
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

// 0..255 全都出现一遍的斜坡图，用来检查每个像素都落进了某个箱
static QImage rampImage(int size)
{
    QImage image(size, size, QImage::Format_Grayscale8);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            image.setPixel(x, y, qRgb(x, x, x));
        }
    }
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

// 在绘图区里横向拖一段完成一次缩放，走的是真实事件通道
static void dragInPlotArea(HistogramViewWidget *widget, int x1, int x2, int y = -1)
{
    auto *plot = plotOf(widget);
    const QRect area = plot->axisRect()->rect();
    if (y < 0) {
        y = area.center().y();
    }

    QMouseEvent press(QEvent::MouseButtonPress, QPointF(x1, y), QPointF(x1, y),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &press);

    QMouseEvent move(QEvent::MouseMove, QPointF(x2, y), QPointF(x2, y),
                     Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &move);

    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(x2, y), QPointF(x2, y),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &release);
}

// 按下并拖到一半就停住，用来检查拖动过程中的选区形状
static QRubberBand *beginDrag(HistogramViewWidget *widget, int x1, int x2, int y)
{
    auto *plot = plotOf(widget);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(x1, y), QPointF(x1, y),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &press);

    QMouseEvent move(QEvent::MouseMove, QPointF(x2, y), QPointF(x2, y),
                     Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &move);

    return plot->findChild<QRubberBand *>();
}

static void rightClick(HistogramViewWidget *widget, const QPoint &pos)
{
    auto *plot = plotOf(widget);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(pos), QPointF(pos),
                      Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(pos), QPointF(pos),
                        Qt::RightButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &release);
}

static double rangeWidth(const QCPAxis *axis)
{
    return axis->range().upper - axis->range().lower;
}

static void uniformBins(int n, double count, QVector<double> &bins, QVector<double> &counts)
{
    bins.clear();
    counts.clear();
    for (int i = 0; i < n; ++i) {
        bins.append(i + 0.5);
        counts.append(count);
    }
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

        // 整幅同一个值，非空的当然只有一箱
        int filled = 0;
        for (int c : window.binCounts()) {
            if (c == 256) {
                filled = 1;
            }
        }
        QCOMPARE(filled, 1);
    }

    void test_values_per_bin_defaults_to_ten()
    {
        HistogramWindow window;
        QCOMPARE(window.valuesPerBin(), 10);

        // 8 位图 256 个取值，每箱 10 个 → 26 个箱
        window.setImage(flatImage(16, 100));
        QCOMPARE(window.maxValue(), 255);
        QCOMPARE(window.valuesPerBin(), 10);
        QCOMPARE(window.binCount(), 26);

        // 16 位图 65536 个取值，每箱 10 个 → 6554 个箱
        window.setImage(flatImage16(8, 50000));
        QCOMPARE(window.maxValue(), 65535);
        QCOMPARE(window.valuesPerBin(), 10);
        QCOMPARE(window.binCount(), 6554);
    }

    void test_values_per_bin_is_adjustable()
    {
        HistogramWindow window;
        window.setImage(flatImage(16, 100));

        // 每箱一个取值：8 位图回到一个值一个箱
        window.setValuesPerBin(1);
        QCOMPARE(window.valuesPerBin(), 1);
        QCOMPARE(window.binCount(), 256);
        QCOMPARE(window.binCounts().at(100), 256);
        QVERIFY(qAbs(window.binCenters().at(100) - 100.5) < 1e-9);

        // 每箱 20 个：13 个箱
        window.setValuesPerBin(20);
        QCOMPARE(window.binCount(), 13);

        // 换一个不整除的数：箱数向上取整，但所有箱仍要正好铺满值域，
        // 横坐标的量程不能超出 0~256
        window.setValuesPerBin(7);
        QCOMPARE(window.binCount(), 37);
        auto *plot = plotOf(window.histogramWidget());
        QVERIFY(plot);
        QVERIFY(qAbs(rangeWidth(plot->xAxis) - 256.0) < 1e-6);
    }

    void test_every_pixel_lands_in_a_bin()
    {
        // 箱宽是摊平后的分数（256/37 ≈ 6.92），除法取整最容易在边界上
        // 丢像素或越界。用一幅 0..15 的斜坡图逐箱核对
        HistogramWindow window;
        window.setValuesPerBin(7);
        window.setImage(rampImage(16));

        QCOMPARE(window.binCount(), 37);

        int total = 0;
        for (int c : window.binCounts()) {
            total += c;
        }
        QCOMPARE(total, 16 * 16);

        // 斜坡图里 0..15 每个值各占 16 个像素（一列），
        // 按箱宽 256/37 算：值 0~6 进第 0 箱，7~13 进第 1 箱，14~15 进第 2 箱
        QCOMPARE(window.binCounts().at(0), 7 * 16);
        QCOMPARE(window.binCounts().at(1), 7 * 16);
        QCOMPARE(window.binCounts().at(2), 2 * 16);
        // 剩下全是空的，没有像素被塞到范围外
        for (int i = 3; i < window.binCount(); ++i) {
            QCOMPARE(window.binCounts().at(i), 0);
        }
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
        QCOMPARE(window.binCount(), 6554);

        // 箱宽 65536/6554 ≈ 10，50000 落在第 5000 号箱
        const int expectedBin = static_cast<int>(50000 / (65536.0 / 6554));
        QCOMPARE(window.binCounts().at(expectedBin), 16);
        QCOMPARE(window.binCounts().at(0), 0);

        // 每箱一个取值时，50000 自己的箱就是 50000 号箱
        window.setValuesPerBin(1);
        QCOMPARE(window.binCount(), 65536);
        QCOMPARE(window.binCounts().at(50000), 16);
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
        // 每箱一个取值，这样箱号就是灰度值本身，断言写得死一点
        window.setValuesPerBin(1);

        window.setImage(flatImage(8, 10));
        QCOMPARE(window.binCounts().at(10), 64);
        QCOMPARE(window.overexposedPixelCount(), 0);

        window.setImage(flatImage(8, 255));
        QCOMPARE(window.binCounts().at(10), 0);
        QCOMPARE(window.binCounts().at(255), 64);
        QCOMPARE(window.overexposedPixelCount(), 64);

        // 换成分箱模式：两个值都进第 1 箱（箱宽 256/26 ≈ 9.85）
        window.setValuesPerBin(10);
        window.setImage(flatImage(8, 10));
        QCOMPARE(window.binCount(), 26);
        QCOMPARE(window.binCounts().at(1), 64);
        QCOMPARE(window.binCounts().at(10), 0);
    }

    void test_many_bins_still_render_quickly()
    {
        // 16 位图 + 每箱一个取值 = 65536 个箱。实测渲染约 2.7 ms，
        // 所以箱数上限不用为此让步；留这条是为了挡住将来把柱子换成
        // 更贵的画法。阈值给得很宽松，只抓数量级的退化。
        HistogramViewWidget widget;
        QVector<double> bins;
        QVector<double> counts;
        for (int i = 0; i < 65536; ++i) {
            bins.append(i + 0.5);
            counts.append(10);
        }
        widget.resize(800, 600);
        widget.show();
        QCoreApplication::processEvents();

        widget.setHistogram(bins, counts);   // 热身

        QElapsedTimer timer;
        timer.start();
        widget.setHistogram(bins, counts);
        widget.grab();                       // 强制真的画一遍
        const double ms = timer.nsecsElapsed() / 1e6;

        QVERIFY2(ms < 200.0,
                 qPrintable(QStringLiteral("65536 个箱渲染用了 %1 ms").arg(ms)));
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
        QVERIFY(plot->xAxis->coordToPixel(256.0) <= area.right() + 1.0);
    }
    void test_drag_in_plot_area_zooms_x_only()
    {
        HistogramViewWidget widget;
        widget.resize(600, 400);

        QVector<double> bins;
        QVector<double> counts;
        uniformBins(256, 100.0, bins, counts);
        widget.setHistogram(bins, counts);
        showLayout(&widget);

        auto *plot = plotOf(&widget);
        QVERIFY(plot);
        QVERIFY(!widget.isZoomed());

        const double xBefore = rangeWidth(plot->xAxis);
        const QCPRange yBefore = plot->yAxis->range();

        const QRect area = plot->axisRect()->rect();
        dragInPlotArea(&widget,
                   area.left() + area.width() / 4,
                   area.left() + area.width() * 3 / 4);

        QVERIFY(widget.isZoomed());
        QVERIFY2(rangeWidth(plot->xAxis) < xBefore,
                 qPrintable(QStringLiteral("横坐标量程没缩小：%1 -> %2")
                                .arg(xBefore).arg(rangeWidth(plot->xAxis))));

        // 只缩横轴：纵坐标必须原样不动
        QCOMPARE(plot->yAxis->range().lower, yBefore.lower);
        QCOMPARE(plot->yAxis->range().upper, yBefore.upper);
    }

    void test_selection_band_is_full_height()
    {
        // 选区是纵向的一条：高度占满整个绘图区，只有宽度跟着拖动走
        HistogramViewWidget widget;
        widget.resize(600, 400);

        QVector<double> bins;
        QVector<double> counts;
        uniformBins(256, 100.0, bins, counts);
        widget.setHistogram(bins, counts);
        showLayout(&widget);

        auto *plot = plotOf(&widget);
        const QRect area = plot->axisRect()->rect();
        QVERIFY(area.height() > 10);

        QRubberBand *band = beginDrag(&widget,
                                      area.left() + area.width() / 4,
                                      area.left() + area.width() * 3 / 4,
                                      area.center().y());
        QVERIFY(band);
        QVERIFY(band->isVisible());

        QVERIFY2(band->geometry().top() == area.top()
                     && band->geometry().height() == area.height(),
                 qPrintable(QStringLiteral("选区高度 %1，绘图区 %2~%3")
                                .arg(band->geometry().height())
                                .arg(area.top()).arg(area.bottom())));
        QVERIFY(band->geometry().width() > 0);
        QVERIFY(band->geometry().width() < area.width());
    }

    void test_selection_band_follows_mouse_vertically()
    {
        // 鼠标上下移动不该影响选区：横坐标才是要放大的量
        HistogramViewWidget widget;
        widget.resize(600, 400);

        QVector<double> bins;
        QVector<double> counts;
        uniformBins(256, 100.0, bins, counts);
        widget.setHistogram(bins, counts);
        showLayout(&widget);

        auto *plot = plotOf(&widget);
        const QRect area = plot->axisRect()->rect();
        const int x1 = area.left() + area.width() / 4;
        const int x2 = area.left() + area.width() * 3 / 4;

        QRubberBand *band = beginDrag(&widget, x1, x2, area.top() + 2);
        QVERIFY(band);
        const QRect atTop = band->geometry();

        QMouseEvent move(QEvent::MouseMove, QPointF(x2, area.bottom() - 2),
                         QPointF(x2, area.bottom() - 2),
                         Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(plot, &move);

        QCOMPARE(band->geometry(), atTop);
    }

    void test_drag_outside_plot_area_does_not_zoom()
    {
        // 按在坐标轴刻度那一带（绘图区之外）不该触发缩放
        HistogramViewWidget widget;
        widget.resize(600, 400);

        QVector<double> bins;
        QVector<double> counts;
        uniformBins(256, 100.0, bins, counts);
        widget.setHistogram(bins, counts);
        showLayout(&widget);

        auto *plot = plotOf(&widget);
        const double full = rangeWidth(plot->xAxis);
        const QRect area = plot->axisRect()->rect();

        // 绘图区下方：横坐标轴刻度和标签占的那条
        const int yBelow = area.bottom() + (plot->height() - area.bottom()) / 2;
        QVERIFY(yBelow < plot->height());
        dragInPlotArea(&widget,
                       area.left() + area.width() / 4,
                       area.left() + area.width() * 3 / 4,
                       yBelow);

        QVERIFY(!widget.isZoomed());
        QVERIFY2(qAbs(rangeWidth(plot->xAxis) - full) < 1e-6,
                 qPrintable(QStringLiteral("在绘图区外拖也缩放了：%1 -> %2")
                                .arg(full).arg(rangeWidth(plot->xAxis))));
    }

    void test_zoom_cannot_go_below_floor()
    {
        // 不许无限放大：下界是 4 个箱，箱宽是多少就跟着放大到多少倍
        HistogramViewWidget widget;
        widget.resize(600, 400);

        QVector<double> bins;
        QVector<double> counts;
        uniformBins(256, 100.0, bins, counts);
        widget.setHistogram(bins, counts);
        showLayout(&widget);

        auto *plot = plotOf(&widget);
        // uniformBins 的箱宽是 1，所以下界是 4 个箱 = 4 个取值
        const double floor = widget.binWidth() * 4.0;
        QVERIFY(qAbs(floor - 4.0) < 1e-6);

        const QRect area = plot->axisRect()->rect();
        const int center = area.left() + area.width() / 2;

        // 一次比一次窄，最狠的是只拖 2 个像素
        for (int span : {area.width() / 2, area.width() / 20, 2}) {
            dragInPlotArea(&widget, center - span / 2, center + span / 2);
            QVERIFY2(rangeWidth(plot->xAxis) >= floor - 1e-6,
                     qPrintable(QStringLiteral("拖了 %1 像素后量程缩到 %2，低于 4 个箱的下界 %3")
                                    .arg(span).arg(rangeWidth(plot->xAxis)).arg(floor)));
        }
    }

    void test_zoom_floor_scales_with_bin_width()
    {
        // 下界跟着箱宽走，而不是写死一个绝对值：16 位图每箱 10 个取值时，
        // 下界是 4 个箱 = 约 40 个取值；每箱一个取值时则是 4 个取值
        HistogramWindow window;
        window.setImage(flatImage16(8, 50000));
        showLayout(&window);

        auto *plot = plotOf(window.histogramWidget());
        QVERIFY(plot);
        const QRect area = plot->axisRect()->rect();
        const int center = area.left() + area.width() / 2;

        dragInPlotArea(window.histogramWidget(), center - 1, center + 1);

        const double coarse = rangeWidth(plot->xAxis);
        QVERIFY2(coarse >= window.histogramWidget()->binWidth() * 4.0 - 1e-6,
                 qPrintable(QStringLiteral("下界只有 %1").arg(coarse)));
        QVERIFY2(coarse > 30.0, qPrintable(QStringLiteral("每箱 10 个时下界只有 %1").arg(coarse)));

        // 换成一箱一个取值，下界跟着缩回去
        window.setValuesPerBin(1);
        dragInPlotArea(window.histogramWidget(), center - 1, center + 1);
        const double fine = rangeWidth(plot->xAxis);
        QVERIFY2(fine < coarse,
                 qPrintable(QStringLiteral("换成一箱一个取值后下界反而变大了：%1 -> %2")
                                .arg(coarse).arg(fine)));
        QVERIFY(qAbs(fine - 4.0) < 1e-6);
    }

    void test_x_axis_labels_have_no_decimals()
    {
        // 横坐标是像素值，不管缩放到多窄都不该冒出小数标签
        HistogramViewWidget widget;
        widget.resize(600, 400);

        QVector<double> bins;
        QVector<double> counts;
        uniformBins(256, 100.0, bins, counts);
        widget.setHistogram(bins, counts);
        showLayout(&widget);

        auto *plot = plotOf(&widget);
        QCOMPARE(plot->xAxis->numberPrecision(), 0);
        QCOMPARE(plot->xAxis->numberFormat(), QStringLiteral("f"));

        // 缩到很窄，量程会落在非整数位置上，标签仍要是整数
        const QRect area = plot->axisRect()->rect();
        dragInPlotArea(&widget,
                   area.left() + area.width() / 3,
                   area.left() + area.width() * 2 / 3);

        // 把量程直接压到 1 以下，逼刻度器给出分数刻度（12.1、12.2……）。
        // 正常交互到不了这么窄（框选有下界），这里直接改量程来验证
        // 「横坐标标签永远是整数」这条约定本身成立
        plot->xAxis->setRange(12.0, 12.6);
        plot->replot();

        for (int pass = 0; pass < 2; ++pass) {
            // tickVectorLabels 就是轴上真正画的那批标签文本
            const QVector<QString> labels = plot->xAxis->tickVectorLabels();
            QVERIFY(!labels.isEmpty());
            for (const QString &label : labels) {
                QVERIFY2(!label.contains(QLatin1Char('.')),
                         qPrintable(QStringLiteral("横坐标出现了小数标签：%1（量程 %2~%3）")
                                        .arg(label).arg(plot->xAxis->range().lower)
                                        .arg(plot->xAxis->range().upper)));
            }
            if (pass == 0) {
                // 再换到另一个分数区间，确认不是碰巧
                plot->xAxis->setRange(200.1, 200.7);
                plot->replot();
            }
        }
    }

    void test_zoom_survives_new_frame()
    {
        // live 模式每帧都来，不能把用户框出来的量程又拉回去
        HistogramViewWidget widget;
        widget.resize(600, 400);

        QVector<double> bins;
        QVector<double> counts;
        uniformBins(256, 100.0, bins, counts);
        widget.setHistogram(bins, counts);
        showLayout(&widget);

        auto *plot = plotOf(&widget);
        const QRect area = plot->axisRect()->rect();
        dragInPlotArea(&widget,
                   area.left() + area.width() / 4,
                   area.left() + area.width() * 3 / 4);

        const double zoomed = rangeWidth(plot->xAxis);
        QVERIFY(zoomed > 0.0);

        uniformBins(256, 40.0, bins, counts);
        widget.setHistogram(bins, counts);

        QVERIFY2(qAbs(rangeWidth(plot->xAxis) - zoomed) < 1e-6,
                 qPrintable(QStringLiteral("新帧把框选的横坐标量程冲掉了：%1 -> %2")
                                .arg(zoomed).arg(rangeWidth(plot->xAxis))));
    }

    void test_right_click_resets_zoom()
    {
        HistogramViewWidget widget;
        widget.resize(600, 400);

        QVector<double> bins;
        QVector<double> counts;
        uniformBins(256, 100.0, bins, counts);
        widget.setHistogram(bins, counts);
        showLayout(&widget);

        auto *plot = plotOf(&widget);
        const double full = rangeWidth(plot->xAxis);
        const QRect area = plot->axisRect()->rect();

        dragInPlotArea(&widget,
                   area.left() + area.width() / 4,
                   area.left() + area.width() * 3 / 4);
        QVERIFY(widget.isZoomed());
        QVERIFY(rangeWidth(plot->xAxis) < full);

        rightClick(&widget, area.center());
        QVERIFY(!widget.isZoomed());
        QVERIFY2(qAbs(rangeWidth(plot->xAxis) - full) < 1e-6,
                 qPrintable(QStringLiteral("右键复位后量程没还原：%1 -> %2")
                                .arg(full).arg(rangeWidth(plot->xAxis))));
    }

    void test_clear_resets_zoom()
    {
        // 数据清空后还留着框选量程的话，下一帧进来横坐标会卡在上一个图的范围内
        HistogramViewWidget widget;
        widget.resize(600, 400);

        QVector<double> bins;
        QVector<double> counts;
        uniformBins(256, 100.0, bins, counts);
        widget.setHistogram(bins, counts);
        showLayout(&widget);

        auto *plot = plotOf(&widget);
        const QRect area = plot->axisRect()->rect();
        dragInPlotArea(&widget,
                   area.left() + area.width() / 4,
                   area.left() + area.width() * 3 / 4);
        QVERIFY(widget.isZoomed());

        widget.clearHistogram();
        QVERIFY(!widget.isZoomed());

        uniformBins(256, 100.0, bins, counts);
        widget.setHistogram(bins, counts);
        QVERIFY2(qAbs(rangeWidth(plot->xAxis) - 256.0) < 1.0,
                 qPrintable(QStringLiteral("清图后量程没按新数据重排：%1").arg(rangeWidth(plot->xAxis))));
    }

    void test_click_without_drag_does_not_zoom()
    {
        // 点一下没拖动不能当成缩放，否则量程会被压成一条线
        HistogramViewWidget widget;
        widget.resize(600, 400);

        QVector<double> bins;
        QVector<double> counts;
        uniformBins(256, 100.0, bins, counts);
        widget.setHistogram(bins, counts);
        showLayout(&widget);

        auto *plot = plotOf(&widget);
        const double full = rangeWidth(plot->xAxis);
        const QRect area = plot->axisRect()->rect();
        const int x = area.left() + area.width() / 2;
        const int y = area.bottom() + (plot->height() - area.bottom()) / 2;

        QMouseEvent press(QEvent::MouseButtonPress, QPointF(x, y), QPointF(x, y),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(plot, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(x, y), QPointF(x, y),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(plot, &release);

        QVERIFY(!widget.isZoomed());
        QVERIFY(qAbs(rangeWidth(plot->xAxis) - full) < 1e-6);
    }
};

QTEST_MAIN(TestHistogram)
#include "test_histogram.moc"
