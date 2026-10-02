#include <QCoreApplication>
#include <QMouseEvent>
#include <QObject>
#include <QSignalSpy>
#include <QTest>
#include <QVector>

#include "SpectrumViewWidget.h"
#include "qcustomplot.h"

static QCustomPlot *plotOf(SpectrumViewWidget *widget)
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
static void showLayout(SpectrumViewWidget *widget)
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
static double intensityInReadout(const QCPItemText *readout)
{
    const QStringList lines = readout->text().split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("I: "))) {
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
        SpectrumViewWidget widget;
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
        const double first = intensityInReadout(readout);
        QVERIFY(first > 0.0);

        // 鼠标不动，只换一帧数据
        QSignalSpy spy(&widget, &SpectrumViewWidget::cursorPosition);
        widget.setData(indices(100), ramp(100, 6000.0));

        QVERIFY(readout->visible());
        const double second = intensityInReadout(readout);
        QVERIFY2(qAbs(second - first) > 100.0,
                 qPrintable(QStringLiteral("读数没有跟着新数据更新：%1 -> %2")
                                .arg(first).arg(second)));

        // 状态栏那一路读的是 cursorPosition，数据刷新也必须重发一次
        QCOMPARE(spy.count(), 1);
        QVERIFY(qAbs(spy.at(0).at(1).toDouble() - second) < 0.5);
    }

    void test_x_stays_put_while_value_changes()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        auto *plot = plotOf(&widget);
        QVERIFY(plot);
        QCPItemText *readout = readoutOf(plot);
        QVERIFY(readout);

        widget.setData(indices(100), ramp(100, 1000.0));
        showLayout(&widget);

        moveMouseOver(plot, plot->axisRect()->rect().left() + 100);
        const QString before = xLineInReadout(readout);
        const double valueBefore = intensityInReadout(readout);

        widget.setData(indices(100), ramp(100, 5000.0));

        // 只有强度那一行变，横坐标那一行必须原样不动
        QCOMPARE(xLineInReadout(readout), before);
        const double valueAfter = intensityInReadout(readout);
        QVERIFY2(qAbs(valueAfter - valueBefore - 4000.0) < 0.5,
                 qPrintable(QStringLiteral("before=%1 after=%2 text='%3'")
                                .arg(valueBefore).arg(valueAfter).arg(readout->text())));
    }

    void test_no_readout_before_first_mouse_move()
    {
        // 反过来也要守住：鼠标从没进过画面，不能凭空冒出一个读数
        SpectrumViewWidget widget;
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
        SpectrumViewWidget widget;
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
        SpectrumViewWidget widget;
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
};

QTEST_MAIN(TestSpectrumCursor)
#include "test_spectrum_cursor.moc"
