// Guards the crosshair readout in the status bar.
//
// The label used to sit there permanently showing "Crosshair: --" whenever no
// crosshair existed, so the status bar always reserved 150 px for a dash.
// It is now hidden until a crosshair is actually placed, which is a behaviour
// only observable on a real MainWindow.
#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QStatusBar>
#include <QTest>
#include <QTimer>

#include "widgets/MainWindow.h"
#include "widgets/display/ImageViewWidget.h"

class TestCrosshairStatusBar : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase() {}

    void test_hidden_until_a_crosshair_exists();
    void test_shown_while_a_crosshair_exists();
    void test_hidden_again_after_clearing();
    void test_reappears_when_a_second_crosshair_is_added();
    void test_capture_mode_never_takes_over_the_crosshair_strip();
    void test_capture_mode_is_shown_in_the_state_label();

private:
    QTimer *m_modalDismiss = nullptr;
};

void TestCrosshairStatusBar::initTestCase()
{
    // No camera is connected, so AppController reports an error and MainWindow
    // answers with a modal message box. Nothing would ever dismiss it.
    m_modalDismiss = new QTimer(this);
    m_modalDismiss->setInterval(50);
    connect(m_modalDismiss, &QTimer::timeout, this, [] {
        if (auto *dialog = QApplication::activeModalWidget()) {
            dialog->close();
        }
    });
    m_modalDismiss->start();
}

// 16x16 灰阶图，第 y 行的值是 y*16。用纯灰是为了让 qGray() 精确等于该值，
// 这样断言读数时不必去猜 Qt 的加权取整
static QImage rampImage16()
{
    QImage image(16, 16, QImage::Format_Grayscale8);
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            const int v = y * 16;
            image.setPixel(x, y, qRgb(v, v, v));
        }
    }
    return image;
}

void TestCrosshairStatusBar::test_hidden_until_a_crosshair_exists()
{
    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();

    // 状态栏本身是在的，所以标签不可见只能是策略所致，不是窗口没显示
    QVERIFY2(window.statusBar()->isVisible(), "status bar itself is not visible");

    auto *label = window.findChild<QLabel *>(QStringLiteral("coordLabel"));
    QVERIFY2(label, "status bar has no coordLabel");
    QVERIFY2(label->isHidden(),
             qPrintable(QStringLiteral("没有十字线时读数标签仍然可见：%1").arg(label->text())));
}

void TestCrosshairStatusBar::test_shown_while_a_crosshair_exists()
{
    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();

    auto *view = window.findChild<ImageViewWidget *>();
    QVERIFY(view);
    view->setImage(rampImage16());

    auto *label = window.findChild<QLabel *>(QStringLiteral("coordLabel"));
    QVERIFY(label);

    view->addCrosshair(3, 4);
    QCoreApplication::processEvents();

    QVERIFY2(!label->isHidden(), "放了十字线之后读数标签还是隐藏的");
    QCOMPARE(label->text(), QString("Crosshair: X: 3, Y: 4, Value: 64"));
}

void TestCrosshairStatusBar::test_hidden_again_after_clearing()
{
    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();

    auto *view = window.findChild<ImageViewWidget *>();
    QVERIFY(view);
    view->setImage(rampImage16());

    auto *label = window.findChild<QLabel *>(QStringLiteral("coordLabel"));
    QVERIFY(label);

    view->addCrosshair(3, 4);
    QVERIFY(!label->isHidden());

    view->clearCrosshairs();
    QCoreApplication::processEvents();

    QVERIFY2(label->isHidden(), "清掉十字线后读数标签还留着");
    QVERIFY2(label->text().isEmpty(),
             qPrintable(QStringLiteral("清掉后仍留有文字：%1").arg(label->text())));
}

void TestCrosshairStatusBar::test_reappears_when_a_second_crosshair_is_added()
{
    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();

    auto *view = window.findChild<ImageViewWidget *>();
    QVERIFY(view);
    view->setImage(rampImage16());

    auto *label = window.findChild<QLabel *>(QStringLiteral("coordLabel"));
    QVERIFY(label);

    // 再放一条：addCrosshair() 内部先清空旧的再发 crosshairMoved，
    // 标签不能停在「隐藏」这一侧
    view->addCrosshair(1, 2);
    view->addCrosshair(5, 6);
    QCoreApplication::processEvents();

    QVERIFY(!label->isHidden());
    QCOMPARE(label->text(), QString("Crosshair: X: 5, Y: 6, Value: 96"));
}

// L / S / B 选采集模式时，读数不能和模式提示叠在一起。
//
// 起因：模式提示走的是 QStatusBar::showMessage()，而它会把常驻 widget 全藏
// 起来、在同一块矩形上画提示；随后 setCrosshairInfo() 无条件 setVisible(true)
// 把读数拉回那块矩形，两段文字就从同一个 x 开始画，叠在一起。离屏量过：提示
// 从 x=2 起画，读数也变成 x=2。
//
// 所以模式提示不能占用常驻带子。现在模式并进 stateLabel（另一格），这条断言
// 盯的就是「常驻带子上没有临时提示」。
void TestCrosshairStatusBar::test_capture_mode_never_takes_over_the_crosshair_strip()
{
    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();

    auto *view = window.findChild<ImageViewWidget *>();
    QVERIFY(view);
    view->setImage(rampImage16());
    QCoreApplication::processEvents();

    auto *label = window.findChild<QLabel *>(QStringLiteral("coordLabel"));
    auto *state = window.findChild<QLabel *>(QStringLiteral("stateLabel"));
    QVERIFY(label);
    QVERIFY(state);

    for (const char *slot : {"onLiveModeTriggered", "onBurstModeTriggered",
                             "onSingleModeTriggered"}) {
        QVERIFY2(QMetaObject::invokeMethod(&window, slot, Qt::DirectConnection),
                 slot);
        QCoreApplication::processEvents();

        // 选了模式之后再摆十字线：这条路径正是原来叠字的那一条
        view->addCrosshair(3, 4);
        QCoreApplication::processEvents();

        QVERIFY2(label->isVisible(),
                 qPrintable(QStringLiteral("选了 %1 之后读数不见了").arg(slot)));
        QCOMPARE(label->text(), QString("Crosshair: X: 3, Y: 4, Value: 64"));

        // QStatusBar 把临时提示画在常驻带子的最左端。读数必须待在 stateLabel
        // 那一格右边，否则两者画在同一块矩形上。
        const int stateRight = state->x() + state->width();
        QVERIFY2(label->x() >= stateRight,
                 qPrintable(QStringLiteral("选了 %1 之后读数落在 x=%2，"
                                           "和提示要画的 x=%3..%4 重叠了")
                                .arg(slot).arg(label->x())
                                .arg(state->x()).arg(stateRight - 1)));

        QVERIFY2(window.statusBar()->currentMessage().isEmpty(),
                 qPrintable(QStringLiteral("模式提示还占着常驻带子：%1")
                                .arg(window.statusBar()->currentMessage())));
    }
}

void TestCrosshairStatusBar::test_capture_mode_is_shown_in_the_state_label()
{
    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();

    // 状态栏上有好几个 QLabel，按文字找「State:」开头的那一格
    QLabel *stateLabel = nullptr;
    for (QLabel *candidate : window.findChildren<QLabel *>()) {
        if (candidate->text().startsWith(QStringLiteral("State:"))) {
            stateLabel = candidate;
            break;
        }
    }
    QVERIFY2(stateLabel, "状态栏里没有 'State:' 那一格");
    QCOMPARE(stateLabel->text(), QString("State: Disconnected"));

    QVERIFY(QMetaObject::invokeMethod(&window, "onBurstModeTriggered",
                                      Qt::DirectConnection));
    QCoreApplication::processEvents();
    QVERIFY2(stateLabel->text().endsWith(QString("Mode: Burst (5)")),
             qPrintable(stateLabel->text()));

    QVERIFY(QMetaObject::invokeMethod(&window, "onLiveModeTriggered",
                                      Qt::DirectConnection));
    QCoreApplication::processEvents();
    QVERIFY2(stateLabel->text().endsWith(QString("Mode: Live")),
             qPrintable(stateLabel->text()));
}

QTEST_MAIN(TestCrosshairStatusBar)
#include "test_crosshair_status_bar.moc"
