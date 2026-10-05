// Guards the acquire-dark-frame burst path in MainWindow.
//
// The burst accumulator used to be initialised with
//     m_darkAccumSum.fill(elementCount, 0u)
// but QList::fill is fill(value, size), so that sized the accumulator to zero
// elements and the very next statement indexed it out of bounds. Nothing
// covered this path, so it shipped. These tests drive the real burst and check
// both that it survives and that the running average is correct.
//
// The last case is about where the burst is *started from*: the acquisition
// controls now live inside the Calibration dialog (reached from
// Process -> Calibration), so that a freshly acquired dark frame can be
// enabled from the same window.
#include <QObject>
#include <QApplication>
#include <QTest>
#include <QImage>
#include <QTimer>
#include <QDialog>
#include <QDateTime>
#include <QDebug>
#include <QCheckBox>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSignalSpy>

#include "widgets/MainWindow.h"
#include "widgets/dialogs/CalibrationDialog.h"
#include "widgets/display/ImageViewWidget.h"
#include "CameraTypes.h"

class TestDarkFrameAcquisition : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void test_burst_does_not_crash();
    void test_burst_averages_pixels();
    void test_burst_handles_odd_and_even_widths();
    void test_acquire_moved_into_calibration_dialog();
    void test_acquired_dark_is_enabled_from_the_same_dialog();

private:
    // Runs a full burst of `frames` Grayscale16 frames whose every pixel is
    // (baseValue + i) for frame i, and returns the running average as shown by
    // the image view. Returns a null image if the view never received one.
    QImage runBurst(MainWindow &window, int w, int h, int frames, int baseValue);

    static CalibrationDialog *openCalibrationDialog(MainWindow &window);

    QTimer *m_modalDismiss = nullptr;
};

void TestDarkFrameAcquisition::initTestCase()
{
    // No camera is connected here, so AppController reports an error and
    // MainWindow answers with a modal message box. Nothing would ever dismiss
    // it, so do it from the harness.
    m_modalDismiss = new QTimer(this);
    m_modalDismiss->setInterval(50);
    connect(m_modalDismiss, &QTimer::timeout, this, [] {
        if (auto *dialog = QApplication::activeModalWidget()) {
            dialog->close();
        }
    });
    m_modalDismiss->start();
}

void TestDarkFrameAcquisition::cleanupTestCase()
{
}

void TestDarkFrameAcquisition::init()
{
}

void TestDarkFrameAcquisition::cleanup()
{
}

QImage TestDarkFrameAcquisition::runBurst(MainWindow &window, int w, int h,
                                          int frames, int baseValue)
{
    // Same entry point as clicking "Acquire" in the Calibration dialog.
    const bool armed = QMetaObject::invokeMethod(&window,
                                                 "onAcquireDarkFrameStartRequested",
                                                 Qt::DirectConnection,
                                                 Q_ARG(int, frames));
    if (!armed) {
        return QImage();
    }

    for (int i = 0; i < frames; ++i) {
        QImage img(w, h, QImage::Format_Grayscale16);
        for (int y = 0; y < h; ++y) {
            auto *row = reinterpret_cast<quint16 *>(img.scanLine(y));
            for (int x = 0; x < w; ++x) {
                row[x] = static_cast<quint16>(baseValue + i);
            }
        }

        ImageData frame;
        frame.image = img;
        frame.timestamp = QDateTime::currentMSecsSinceEpoch();
        frame.frameNumber = i;

        QMetaObject::invokeMethod(&window, "onCameraFrameReady",
                                  Qt::DirectConnection, Q_ARG(ImageData, frame));

        // updateDisplay() throttles to one frame per MIN_FRAME_INTERVAL_MS, so
        // the view only picks up every few frames without this.
        QTest::qWait(40);
    }

    auto *view = window.findChild<ImageViewWidget *>();
    return view ? view->image() : QImage();
}

void TestDarkFrameAcquisition::test_burst_does_not_crash()
{
    // Before the fix this aborted inside the accumulator loop on the first
    // frame (Q_ASSERT in a debug build, a null-pointer write in a release one).
    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();

    const QImage shown = runBurst(window, 64, 48, 10, 1000);

    QVERIFY2(!shown.isNull(), "image view never received the averaged frame");
    QCOMPARE(shown.width(), 64);
    QCOMPARE(shown.height(), 48);
}

void TestDarkFrameAcquisition::test_burst_averages_pixels()
{
    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();

    // Frames hold 1000..1009, so the mean is 1004.5 and the code rounds half up.
    constexpr int kFrames = 10;
    constexpr int kExpected = 1005;

    const QImage shown = runBurst(window, 64, 48, kFrames, 1000);
    QVERIFY(!shown.isNull());

    for (int y = 0; y < shown.height(); ++y) {
        const auto *row = reinterpret_cast<const quint16 *>(shown.constScanLine(y));
        for (int x = 0; x < shown.width(); ++x) {
            QCOMPARE(int(row[x]), kExpected);
        }
    }
}

void TestDarkFrameAcquisition::test_burst_handles_odd_and_even_widths()
{
    // QImage pads Grayscale16 rows to a 4-byte boundary, so odd widths have a
    // row stride wider than width*2. The burst must survive both; this asserts
    // the crash is gone, not the pixel values (see the row-stride note in
    // MainWindow::onCameraFrameReady).
    for (int width : {64, 63, 641}) {
        MainWindow window;
        window.resize(1130, 870);
        window.show();
        QCoreApplication::processEvents();

        const QImage shown = runBurst(window, width, 48, 5, 500);
        QVERIFY2(!shown.isNull(),
                 qPrintable(QStringLiteral("no frame for width %1").arg(width)));
        QCOMPARE(shown.width(), width);
    }
}

CalibrationDialog *TestDarkFrameAcquisition::openCalibrationDialog(MainWindow &window)
{
    if (!QMetaObject::invokeMethod(&window, "on_actionCalibration_triggered",
                                   Qt::DirectConnection)) {
        return nullptr;
    }
    QCoreApplication::processEvents();
    return window.findChild<CalibrationDialog *>();
}

void TestDarkFrameAcquisition::test_acquire_moved_into_calibration_dialog()
{
    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();

    QMenuBar *bar = window.menuBar();
    QVERIFY(bar);

    QMenu *processMenu = nullptr;
    QMenu *cameraMenu = nullptr;
    for (QMenu *menu : bar->findChildren<QMenu *>()) {
        if (menu->title() == "&Process") {
            processMenu = menu;
        } else if (menu->title() == "&Camera") {
            cameraMenu = menu;
        }
        QVERIFY2(menu->title() != "&Post-Process",
                 "Post-Process 菜单没有改名为 Process");
    }

    QVERIFY2(processMenu, "找不到 Process 菜单");
    QVERIFY(cameraMenu);

    bool hasCalibration = false;
    for (QAction *action : processMenu->actions()) {
        if (action->text().contains(QStringLiteral("Calibration"))) {
            hasCalibration = true;
        }
    }
    QVERIFY2(hasCalibration, "Process 菜单里没有 Calibration");

    // 采集暗帧不再是 Camera 菜单下的一项
    for (QAction *action : cameraMenu->actions()) {
        QVERIFY2(!action->text().contains(QStringLiteral("Dark"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("Camera 菜单里还留着：%1").arg(action->text())));
    }

    // 入口搬进了 Calibration 对话框
    auto *dialog = openCalibrationDialog(window);
    QVERIFY(dialog);
    QVERIFY2(dialog->findChild<QPushButton *>(QStringLiteral("acquireButton")),
             "Calibration 对话框里没有 Acquire 按钮");
}

void TestDarkFrameAcquisition::test_acquired_dark_is_enabled_from_the_same_dialog()
{
    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();

    auto *dialog = openCalibrationDialog(window);
    QVERIFY(dialog);

    auto *pathField = dialog->findChild<QLineEdit *>();
    auto *enableCheck = dialog->findChild<QCheckBox *>();
    auto *okButton = dialog->findChild<QPushButton *>(QStringLiteral("okButton"));
    QVERIFY(pathField);
    QVERIFY(enableCheck);
    QVERIFY(okButton);

    // 还没采过：不能冒充已经有暗帧
    QVERIFY(!pathField->placeholderText().contains(QStringLiteral("in-memory")));

    constexpr int kFrames = 4;
    const QImage shown = runBurst(window, 32, 24, kFrames, 100);
    QVERIFY(!shown.isNull());

    // 窗口不用关掉重开，采完当场就显示「已经采到 N 帧平均」
    QVERIFY2(pathField->placeholderText().contains(QStringLiteral("in-memory dark frame")),
             qPrintable(pathField->placeholderText()));
    QVERIFY2(pathField->placeholderText().contains(QStringLiteral("4-frame average")),
             qPrintable(pathField->placeholderText()));
    QVERIFY(pathField->text().isEmpty());

    // Enable 就在同一个窗口里，采完立刻能勾上并生效
    QVERIFY(enableCheck->isEnabled());
    QSignalSpy applied(dialog, &CalibrationDialog::applied);
    enableCheck->click();
    okButton->click();

    QCOMPARE(applied.count(), 1);
    QCOMPARE(applied.at(0).at(0).toBool(), true);
    // 走的是内存里的暗帧，路径必须是空的
    QCOMPARE(applied.at(0).at(1).toString(), QString());
}

QTEST_MAIN(TestDarkFrameAcquisition)
#include "test_dark_frame_acquisition.moc"
