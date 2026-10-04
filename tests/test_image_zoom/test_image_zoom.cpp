#include <QCoreApplication>
#include <QImage>
#include <QMouseEvent>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QTest>
#include <cmath>

#include "ImageViewWidget.h"
#include "qcustomplot.h"

static QCustomPlot *plotOf(ImageViewWidget *widget)
{
    return widget->findChild<QCustomPlot *>();
}

// 色图不是 plot 的 QObject 子对象，只能从 plottable 列表里认出来
static QCPColorMap *colorMapOf(QCustomPlot *plot)
{
    for (int i = 0; i < plot->plottableCount(); ++i) {
        if (auto *cm = dynamic_cast<QCPColorMap *>(plot->plottable(i))) {
            return cm;
        }
    }
    return nullptr;
}

static void showLayout(ImageViewWidget *widget)
{
    widget->show();
    QCoreApplication::processEvents();
    widget->findChild<QCustomPlot *>()->replot();
    QCoreApplication::processEvents();
}

static void sendPress(QCustomPlot *plot, const QPoint &pos, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QMouseEvent event(QEvent::MouseButtonPress, QPointF(pos), QPointF(pos),
                      Qt::LeftButton, Qt::LeftButton, modifiers);
    QCoreApplication::sendEvent(plot, &event);
}

static void sendMove(QCustomPlot *plot, const QPoint &pos)
{
    QMouseEvent event(QEvent::MouseMove, QPointF(pos), QPointF(pos),
                      Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &event);
}

static void sendRelease(QCustomPlot *plot, const QPoint &pos)
{
    QMouseEvent event(QEvent::MouseButtonRelease, QPointF(pos), QPointF(pos),
                      Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &event);
    QCoreApplication::processEvents();
    plot->replot();
}

// 在图像上按一下、拖到 releasePos 再松手。releasePos 允许落在 plot 控件之外：
// 按下时 Qt 建立了隐式鼠标捕获，松手事件照样送到 plot，坐标也是相对 plot 的。
static void rubberBandZoom(ImageViewWidget *widget, const QPoint &pressPos, const QPoint &releasePos)
{
    auto *plot = plotOf(widget);
    sendPress(plot, pressPos);
    sendMove(plot, releasePos);
    sendRelease(plot, releasePos);
    plot->replot();
    QCoreApplication::processEvents();
}

static void ctrlClick(ImageViewWidget *widget, const QPoint &pos)
{
    auto *plot = plotOf(widget);
    sendPress(plot, pos, Qt::ControlModifier);
    plot->replot();
    QCoreApplication::processEvents();
}

static void hoverOver(ImageViewWidget *widget, const QPoint &pos)
{
    auto *plot = plotOf(widget);
    QMouseEvent event(QEvent::MouseMove, QPointF(pos), QPointF(pos),
                      Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &event);
}

static const int kImageWidth = 400;
static const int kImageHeight = 60;

// 400 宽的图放进近千像素宽的控件里，缩放前后裁剪出的区域都远小于视口，
// updateDisplayData() 里的降采样不会触发 —— 于是「纹素数 == 区间跨度」可以严格相等。
// 换成比视口宽得多的图，这条断言就得改成允许整除。
static QImage testImage()
{
    QImage image(kImageWidth, kImageHeight, QImage::Format_Grayscale8);
    for (int y = 0; y < kImageHeight; ++y) {
        for (int x = 0; x < kImageWidth; ++x) {
            image.setPixel(x, y, (x * 7 + y * 13) % 256);
        }
    }
    return image;
}

class TestImageZoom : public QObject
{
    Q_OBJECT

private:
    // 轴范围、裁剪出来的数据、色图声明的区间三者必须指向同一块区域。
    // 只要有一个跑出去，读数就和画面对不上。
    void verifyRangesConsistent(ImageViewWidget *widget)
    {
        auto *plot = plotOf(widget);
        auto *cm = colorMapOf(plot);
        QVERIFY(cm);

        const QImage &original = widget->image();
        const QCPRange xr = plot->xAxis->range();
        const QCPRange yr = plot->yAxis->range();

        QVERIFY2(xr.lower >= 0 && xr.upper <= original.width(),
                 qPrintable(QStringLiteral("X 轴范围 [%1, %2] 越出图像宽度 %3")
                                .arg(xr.lower).arg(xr.upper).arg(original.width())));
        QVERIFY2(yr.lower >= 0 && yr.upper <= original.height(),
                 qPrintable(QStringLiteral("Y 轴范围 [%1, %2] 越出图像高度 %3")
                                .arg(yr.lower).arg(yr.upper).arg(original.height())));

        // updateColorMap() 拿轴范围当色图的 key/value 区间
        QCOMPARE(cm->data()->keyRange().lower, xr.lower);
        QCOMPARE(cm->data()->keyRange().upper, xr.upper);
        QCOMPARE(cm->data()->valueRange().lower, yr.lower);
        QCOMPARE(cm->data()->valueRange().upper, yr.upper);

        // 纹素要能铺满它自己声明的那个区间。少于此就说明数据被夹回图像内、
        // 区间却还留在图像外 —— 那正是图像被拉伸铺满、读数整体偏掉的成因。
        QCOMPARE(cm->data()->keySize(), std::ceil(xr.upper) - std::floor(xr.lower));
        QCOMPARE(cm->data()->valueSize(), std::ceil(yr.upper) - std::floor(yr.lower));
    }

    // 画面上下 10%~90% 的每一处都得读得到像素值。
    // 区间越界时中段会整段读到空白 —— 那是「图看着正常、读数却是空的」最直观的症状。
    void verifyReadoutCoversPlot(ImageViewWidget *widget)
    {
        auto *plot = plotOf(widget);
        const QRect area = plot->axisRect()->rect();
        const int height = widget->image().height();

        QSignalSpy spy(widget, &ImageViewWidget::pixelInfo);

        for (int i = 1; i <= 9; ++i) {
            const QPoint pos(area.center().x(), area.top() + area.height() * i / 10);
            hoverOver(widget, pos);

            QVERIFY2(spy.count() == i,
                     qPrintable(QStringLiteral("画面 %1%% 高度处没有读数（axisRect 高 %2，轴范围 Y[%3, %4]）")
                                    .arg(i * 10).arg(area.height())
                                    .arg(plot->yAxis->range().lower)
                                    .arg(plot->yAxis->range().upper)));

            const QPointF coords = spy.last().at(0).toPointF();
            QVERIFY2(coords.y() >= 0 && coords.y() < height,
                     qPrintable(QStringLiteral("读数 Y=%1 越出图像高度 %2").arg(coords.y()).arg(height)));
        }
    }

    ImageViewWidget *makeWidget()
    {
        auto *widget = new ImageViewWidget;
        widget->resize(1000, 900);
        widget->setImage(testImage());
        showLayout(widget);
        return widget;
    }

private slots:
    void init() {}
    void cleanup() {}

    void test_zoom_inside_image()
    {
        // 对照组：选框完全落在图像内，行为不该变
        QScopedPointer<ImageViewWidget> widget(makeWidget());
        auto *plot = plotOf(widget.data());

        const QRect area = plot->axisRect()->rect();
        const QPoint press(area.left() + area.width() / 4, area.center().y());

        rubberBandZoom(widget.data(), press, QPoint(press.x() + 120, press.y() + 4));

        // 确实缩放了，而且缩到了图像内部
        QVERIFY(plot->xAxis->range().size() < kImageWidth);
        QVERIFY(plot->yAxis->range().size() < kImageHeight);
        verifyRangesConsistent(widget.data());
        verifyReadoutCoversPlot(widget.data());
    }

    void test_zoom_dragged_below_image_stays_in_bounds()
    {
        // 用户报告的场景：在图里按下，往下拖到图像外面才松手。
        // 修之前轴范围会一路外推到图像下方，图像却被夹回图内拉伸铺满整个绘图区，
        // 读数和十字线于是整体偏掉。
        QScopedPointer<ImageViewWidget> widget(makeWidget());
        auto *plot = plotOf(widget.data());

        const QRect area = plot->axisRect()->rect();
        const QPoint press(area.left() + area.width() / 4, area.center().y());

        rubberBandZoom(widget.data(), press, QPoint(press.x() + 120, plot->height() + 200));

        verifyRangesConsistent(widget.data());
        verifyReadoutCoversPlot(widget.data());
    }

    void test_zoom_dragged_above_image_stays_in_bounds()
    {
        // 往上拖同理。这一侧还顺带覆盖了 Y 轴 rangeReversed 下的上下次序
        QScopedPointer<ImageViewWidget> widget(makeWidget());
        auto *plot = plotOf(widget.data());

        const QRect area = plot->axisRect()->rect();
        const QPoint press(area.left() + area.width() / 4, area.center().y());

        rubberBandZoom(widget.data(), press, QPoint(press.x() + 120, -200));

        verifyRangesConsistent(widget.data());
        verifyReadoutCoversPlot(widget.data());
    }

    void test_zoom_dragged_beside_image_stays_in_bounds()
    {
        // 横向同理：往右拖出图像。Y 也得给一点跨度，否则选框退化成一条竖线
        QScopedPointer<ImageViewWidget> widget(makeWidget());
        auto *plot = plotOf(widget.data());

        const QRect area = plot->axisRect()->rect();
        const QPoint press(area.left() + area.width() / 4, area.center().y());

        rubberBandZoom(widget.data(), press, QPoint(plot->width() + 200, press.y() + 4));

        verifyRangesConsistent(widget.data());
        verifyReadoutCoversPlot(widget.data());
    }

    void test_crosshair_covers_whole_plot_after_outside_drag()
    {
        // 十字线是按数据 0..height 画的，越界的轴范围会把它裁成一小截，
        // 看上去就像「只有上半部分是图像的」
        QScopedPointer<ImageViewWidget> widget(makeWidget());
        auto *plot = plotOf(widget.data());

        const QRect area = plot->axisRect()->rect();
        const QPoint press(area.left() + area.width() / 4, area.center().y());

        rubberBandZoom(widget.data(), press, QPoint(press.x() + 120, plot->height() + 200));
        plot->replot();

        const QRect after = plot->axisRect()->rect();
        ctrlClick(widget.data(), after.center());
        plot->replot();

        QCPItemLine *vertical = nullptr;
        for (QObject *object : plot->findChildren<QCPItemLine *>()) {
            auto *line = static_cast<QCPItemLine *>(object);
            if (qAbs(line->start->coords().x() - line->end->coords().x()) < 1) {
                vertical = line;
            }
        }
        QVERIFY(vertical);

        const double top = plot->yAxis->coordToPixel(vertical->start->coords().y());
        const double bottom = plot->yAxis->coordToPixel(vertical->end->coords().y());
        const double span = qAbs(bottom - top);

        QVERIFY2(span >= after.height() * 0.98,
                 qPrintable(QStringLiteral("十字线竖段只占绘图区 %1%（绘图区高 %2，轴范围 Y[%3, %4]）")
                                .arg(100.0 * span / after.height())
                                .arg(after.height())
                                .arg(plot->yAxis->range().lower)
                                .arg(plot->yAxis->range().upper)));
    }

    void test_vertical_drag_leaves_view_untouched()
    {
        // 纯竖直拖动选框宽度为零，X 区间退化。此时不该出现「只改了 Y、没改 X」的半截状态
        QScopedPointer<ImageViewWidget> widget(makeWidget());
        auto *plot = plotOf(widget.data());

        const QRect area = plot->axisRect()->rect();
        const QPoint press(area.left() + area.width() / 4, area.center().y());

        rubberBandZoom(widget.data(), press, QPoint(press.x(), area.center().y() + 40));

        QCOMPARE(plot->xAxis->range().lower, 0.0);
        QCOMPARE(plot->xAxis->range().upper, static_cast<double>(kImageWidth));
        QCOMPARE(plot->yAxis->range().lower, 0.0);
        QCOMPARE(plot->yAxis->range().upper, static_cast<double>(kImageHeight));
        verifyRangesConsistent(widget.data());
    }
};

QTEST_MAIN(TestImageZoom)
#include "test_image_zoom.moc"
