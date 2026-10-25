#include <QCoreApplication>
#include <QMouseEvent>
#include <QObject>
#include <QSignalSpy>
#include <QTest>
#include <QVector>

#include <limits>

#include "LineViewWidget.h"
#include "qcustomplot.h"

static QCustomPlot *plotOf(LineViewWidget *widget)
{
    return widget->findChild<QCustomPlot *>();
}

// overlay 上只有一个 QCPItemText（读数框）和一个 QCPItemLine（竖线）
static QCPItemText *readoutOf(QCustomPlot *plot)
{
    for (int i = 0; i < plot->itemCount(); ++i) {
        if (auto *text = dynamic_cast<QCPItemText *>(plot->item(i))) {
            return text;
        }
    }
    return nullptr;
}

// 走真实的鼠标事件，而不是调私有函数：这次要验的恰恰是「鼠标没动」这条路
// 没 show 过的话 QCustomPlot 还没排版，axisRect 是空的，鼠标坐标会一律落到数据
// 起点，测的就不是「某个像素对应哪个数据点」了
static void showLayout(LineViewWidget *widget)
{
    widget->show();
    QCoreApplication::processEvents();
    widget->findChild<QCustomPlot *>()->replot();
}

static void moveMouseOver(QCustomPlot *plot, int x)
{
    const QRect area = plot->axisRect()->rect();
    const int y = area.top() + area.height() / 2;
    QMouseEvent event(QEvent::MouseMove, QPointF(x, y), QPointF(x, y),
                      Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &event);
}

static QVector<double> ramp(int size, double offset)
{
    QVector<double> v(size);
    for (int i = 0; i < size; ++i) {
        v[i] = offset + i;
    }
    return v;
}

static QVector<double> indices(int size)
{
    QVector<double> v(size);
    for (int i = 0; i < size; ++i) {
        v[i] = i;
    }
    return v;
}

// 读数框第二行是 "I: <值>"
static double valueInReadout(const QCPItemText *readout)
{
    const QStringList lines = readout->text().split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("Y: "))) {
            return line.mid(3).toDouble();
        }
    }
    return -1.0;
}

static QString xLineInReadout(const QCPItemText *readout)
{
    return readout->text().split(QLatin1Char('\n')).value(0);
}

// 光标读数在「数据变了但鼠标没动」时也必须跟着更新 —— live 模式下一直挂着
// 上一帧的数值，读数就成了假数据。
class TestSpectrumCursor : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {}
    void cleanupTestCase() {}
    void init() {}
    void cleanup() {}

    void test_data_refresh_updates_readout()
    {
        LineViewWidget widget;
        widget.resize(800, 600);

        auto *plot = plotOf(&widget);
        QVERIFY(plot);
        QCPItemText *readout = readoutOf(plot);
        QVERIFY(readout);

        // 起点给个非零底：读数落在哪个数据点取决于控件有没有完成布局，
        // 但无论落在哪一点数值都应为正，断言才不会因为布局而误伤
        widget.setData(indices(100), ramp(100, 1000.0));
        showLayout(&widget);

        moveMouseOver(plot, plot->axisRect()->rect().left() + plot->axisRect()->rect().width() / 2);
        QVERIFY(readout->visible());
        const double first = valueInReadout(readout);
        QVERIFY(first > 0.0);

        // 鼠标不动，只换一帧数据
        QSignalSpy spy(&widget, &LineViewWidget::cursorPosition);
        widget.setData(indices(100), ramp(100, 6000.0));

        QVERIFY(readout->visible());
        const double second = valueInReadout(readout);
        QVERIFY2(qAbs(second - first) > 100.0,
                 qPrintable(QStringLiteral("读数没有跟着新数据更新：%1 -> %2")
                                .arg(first).arg(second)));

        // 状态栏那一路读的是 cursorPosition，数据刷新也必须重发一次
        QCOMPARE(spy.count(), 1);
        QVERIFY(qAbs(spy.at(0).at(1).toDouble() - second) < 0.5);
    }

    void test_x_stays_put_while_value_changes()
    {
        LineViewWidget widget;
        widget.resize(800, 600);

        auto *plot = plotOf(&widget);
        QVERIFY(plot);
        QCPItemText *readout = readoutOf(plot);
        QVERIFY(readout);

        widget.setData(indices(100), ramp(100, 1000.0));
        showLayout(&widget);

        moveMouseOver(plot, plot->axisRect()->rect().left() + 100);
        const QString before = xLineInReadout(readout);
        const double valueBefore = valueInReadout(readout);

        widget.setData(indices(100), ramp(100, 5000.0));

        // 只有强度那一行变，横坐标那一行必须原样不动
        QCOMPARE(xLineInReadout(readout), before);
        const double valueAfter = valueInReadout(readout);
        QVERIFY2(qAbs(valueAfter - valueBefore - 4000.0) < 0.5,
                 qPrintable(QStringLiteral("before=%1 after=%2 text='%3'")
                                .arg(valueBefore).arg(valueAfter).arg(readout->text())));
    }

    void test_no_readout_before_first_mouse_move()
    {
        // 反过来也要守住：鼠标从没进过画面，不能凭空冒出一个读数
        LineViewWidget widget;
        widget.resize(800, 600);

        auto *plot = plotOf(&widget);
        QVERIFY(plot);
        QCPItemText *readout = readoutOf(plot);
        QVERIFY(readout);

        widget.setData(indices(100), ramp(100, 0.0));
        widget.setData(indices(100), ramp(100, 1000.0));
        widget.setData(indices(100), ramp(100, 2000.0));

        QVERIFY(!readout->visible());
    }

    void test_readout_hidden_after_mouse_left()
    {
        LineViewWidget widget;
        widget.resize(800, 600);

        auto *plot = plotOf(&widget);
        QVERIFY(plot);
        QCPItemText *readout = readoutOf(plot);
        QVERIFY(readout);

        widget.setData(indices(100), ramp(100, 1000.0));
        showLayout(&widget);

        moveMouseOver(plot, plot->axisRect()->rect().left() + 100);
        QVERIFY(readout->visible());

        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(plot, &leave);
        QVERIFY(!readout->visible());

        // 鼠标已经离开，后续新数据不该再把读数叫醒
        widget.setData(indices(100), ramp(100, 9000.0));
        QVERIFY(!readout->visible());
    }

    void test_clear_data_resets_cursor()
    {
        LineViewWidget widget;
        widget.resize(800, 600);

        auto *plot = plotOf(&widget);
        QVERIFY(plot);
        QCPItemText *readout = readoutOf(plot);
        QVERIFY(readout);

        widget.setData(indices(100), ramp(100, 1000.0));
        showLayout(&widget);

        moveMouseOver(plot, plot->axisRect()->rect().left() + 100);
        QVERIFY(readout->visible());

        widget.clearData();
        QVERIFY(!readout->visible());

        // 数据清空后光标状态也要一起清掉，新数据进来时别再冒出旧读数
        widget.setData(indices(100), ramp(100, 100.0));
        QVERIFY(!readout->visible());
    }

    void test_cursor_snaps_to_nearest_sample_on_non_integer_x()
    {
        LineViewWidget widget;
        widget.resize(800, 600);

        auto *plot = plotOf(&widget);
        QVERIFY(plot);
        QCPItemText *readout = readoutOf(plot);
        QVERIFY(readout);

        // 非整数、非等间距的 x：时间轴就是这种形状。把 dataX 四舍五入当下标用
        // 会取到完全无关的点（甚至越界钳到最后一个点）
        const QVector<double> x = { 10.0, 11.5, 13.2, 15.0, 18.0 };
        const QVector<double> y = { 100.0, 200.0, 300.0, 400.0, 500.0 };
        widget.setData(x, y);
        showLayout(&widget);

        const QRect area = plot->axisRect()->rect();
        const int probeOffset = 200;
        moveMouseOver(plot, area.left() + probeOffset);

        // 光标下的原始数据 x，在测试这边独立算一次最近点
        const double rawX = plot->xAxis->pixelToCoord(area.left() + probeOffset);
        int nearest = 0;
        double best = std::numeric_limits<double>::max();
        for (int i = 0; i < x.size(); ++i) {
            const double distance = qAbs(x[i] - rawX);
            if (distance < best) {
                best = distance;
                nearest = i;
            }
        }

        const QStringList lines = readout->text().split(QLatin1Char('\n'));
        QCOMPARE(lines.value(0), QStringLiteral("X: %1").arg(x[nearest], 0, 'f', 1));
        QCOMPARE(lines.value(1), QStringLiteral("Y: %1").arg(y[nearest], 0, 'f', 0));
    }

    void test_cursor_decimals_are_configurable()
    {
        LineViewWidget widget;
        widget.resize(800, 600);
        widget.setData({ 0.0, 1.0, 2.0, 3.0 }, { 100.25, 200.25, 300.25, 400.25 });
        widget.setCursorDecimals(2, 3);
        showLayout(&widget);

        auto *plot = plotOf(&widget);
        QVERIFY(plot);
        QCPItemText *readout = readoutOf(plot);
        QVERIFY(readout);

        // 最左边落在第一个采样点上
        moveMouseOver(plot, plot->axisRect()->rect().left() + 2);
        QCOMPARE(readout->text(), QStringLiteral("X: 0.00\nY: 100.250"));
    }
};

QTEST_MAIN(TestSpectrumCursor)
#include "test_spectrum_cursor.moc"
