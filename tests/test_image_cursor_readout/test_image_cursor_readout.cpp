#include <QCoreApplication>
#include <QImage>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QTest>
#include <QToolTip>

#include "ImageViewWidget.h"
#include "qcustomplot.h"

static QCustomPlot *plotOf(ImageViewWidget *widget)
{
    return widget->findChild<QCustomPlot *>();
}

// 整幅同一个灰度值的图：读数里的 Value 就是这个值，断言可以写得很死，
// 不受鼠标落在哪个像素的影响
static QImage flatImage(int size, int value)
{
    QImage image(size, size, QImage::Format_Grayscale8);
    image.fill(static_cast<uchar>(value));
    return image;
}

// 走真实的鼠标事件，而不是调私有函数：这次要验的恰恰是「鼠标没动」这条路
// 没 show 过的话 QCustomPlot 还没排版，axisRect 是空的
static void showLayout(ImageViewWidget *widget)
{
    widget->show();
    QCoreApplication::processEvents();
    widget->findChild<QCustomPlot *>()->replot();
}

// ImageViewWidget 把 plot 上收到自己的事件过滤器里，再转发给 mouseMoveEvent，
// 所以事件发给 plot 才等价于用户在画面上移动鼠标
static void moveMouseOver(QCustomPlot *plot, const QPoint &pos)
{
    QMouseEvent event(QEvent::MouseMove, QPointF(pos), QPointF(pos),
                      Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &event);
}

static void leaveWidget(QCustomPlot *plot)
{
    QEvent leave(QEvent::Leave);
    QCoreApplication::sendEvent(plot, &leave);
}

// Qt 在鼠标悬停后弹出 tooltip。这里手动弹一次，模拟「用户已经把鼠标停在画面上」
static void popupTooltip(ImageViewWidget *widget, const QPoint &pos)
{
    QToolTip::showText(widget->mapToGlobal(pos), widget->toolTip(), widget);
    QCoreApplication::processEvents();
}

static QString valuePart(const QString &tooltip)
{
    return tooltip.section(QLatin1String("Value: "), 1);
}

static QString coordPart(const QString &tooltip)
{
    return tooltip.left(tooltip.indexOf(QLatin1String(", Value:")));
}

// 光标读数在「新一帧到了但鼠标没动」时也必须换字 —— live 模式下
// 不换的话，挂在屏幕上的 tooltip 一直显示上一帧的数值。
class TestImageCursorReadout : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {}
    void cleanupTestCase()
    {
        QToolTip::hideText();
    }
    void init()
    {
        QToolTip::hideText();
    }
    void cleanup()
    {
        QToolTip::hideText();
    }

    void test_tooltip_follows_new_frame()
    {
        ImageViewWidget widget;
        widget.resize(400, 300);
        widget.setImage(flatImage(64, 40));
        showLayout(&widget);

        auto *plot = plotOf(&widget);
        QVERIFY(plot);
        const QRect area = plot->axisRect()->rect();
        QVERIFY(area.width() > 20);

        const QPoint pos = area.center();
        moveMouseOver(plot, pos);
        QCOMPARE(valuePart(widget.toolTip()), QStringLiteral("40"));

        // tooltip 已经弹出来了
        popupTooltip(&widget, pos);
        QCOMPARE(QToolTip::text(), widget.toolTip());

        // 鼠标不动，只换一帧数据
        QSignalSpy spy(&widget, &ImageViewWidget::pixelInfo);
        widget.setImage(flatImage(64, 190));

        QCOMPARE(widget.toolTip().section(QLatin1String("Value: "), 1),
                 QStringLiteral("190"));

        // 真正显示在屏幕上的那条也得换字。setToolTip() 本身不重画，
        // 这一条就是防着「属性更新了、屏幕上还是旧值」的
        QCOMPARE(QToolTip::text(), widget.toolTip());
        QVERIFY2(QToolTip::text().contains(QStringLiteral("Value: 190")),
                 qPrintable(QStringLiteral("屏幕上的 tooltip 没更新：%1")
                                .arg(QToolTip::text())));

        // 图没变过，坐标那两行必须原样不动
        QCOMPARE(coordPart(QToolTip::text()), coordPart(widget.toolTip()));

        QCOMPARE(spy.count(), 1);
    }

    void test_no_tooltip_before_first_mouse_move()
    {
        // 反过来也要守住：鼠标从没进过画面，不能凭空冒出一个读数
        ImageViewWidget widget;
        widget.resize(400, 300);
        widget.setImage(flatImage(64, 40));
        showLayout(&widget);

        widget.setImage(flatImage(64, 190));
        widget.setImage(flatImage(64, 77));

        QVERIFY(widget.toolTip().isEmpty());
    }

    void test_tooltip_cleared_after_mouse_left()
    {
        ImageViewWidget widget;
        widget.resize(400, 300);
        widget.setImage(flatImage(64, 40));
        showLayout(&widget);

        auto *plot = plotOf(&widget);
        QVERIFY(plot);

        const QPoint pos = plot->axisRect()->rect().center();
        moveMouseOver(plot, pos);
        popupTooltip(&widget, pos);
        QVERIFY(!widget.toolTip().isEmpty());

        leaveWidget(plot);
        QVERIFY(widget.toolTip().isEmpty());

        // 鼠标已经离开，后续新数据不该再把读数叫醒
        widget.setImage(flatImage(64, 190));
        QVERIFY(widget.toolTip().isEmpty());
    }

    void test_no_tooltip_outside_image_area()
    {
        // 鼠标停在绘图区外的留白上时同样不该有读数，
        // 而且这种状态要能压住后续新数据
        ImageViewWidget widget;
        widget.resize(400, 300);
        widget.setImage(flatImage(64, 40));
        showLayout(&widget);

        auto *plot = plotOf(&widget);
        QVERIFY(plot);
        QVERIFY(plot->axisRect()->rect().left() > 0);

        // 图像按比例居中，左边会留出空白
        moveMouseOver(plot, QPoint(2, plot->axisRect()->rect().center().y()));
        QVERIFY(widget.toolTip().isEmpty());

        widget.setImage(flatImage(64, 190));
        QVERIFY(widget.toolTip().isEmpty());
    }
};

QTEST_MAIN(TestImageCursorReadout)
#include "test_image_cursor_readout.moc"
