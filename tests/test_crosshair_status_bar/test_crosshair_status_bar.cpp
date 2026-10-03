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

QTEST_MAIN(TestCrosshairStatusBar)
#include "test_crosshair_status_bar.moc"
