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
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTemporaryDir>

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
    void test_burst_request_above_the_limit_is_clamped();
    void test_acquired_dark_is_enabled_from_the_same_dialog();
    void test_flat_acquire_uses_dialog_slot_and_exposure();
    void test_acquired_reference_can_be_saved_to_disk();

private:
    // Runs a full burst of `frames` Grayscale16 frames whose every pixel is
    // (baseValue + i) for frame i, and returns the running average as shown by
    // the image view. Returns a null image if the view never received one.
    QImage runBurst(MainWindow &window, int w, int h, int frames, int baseValue);

    // Delivers `frames` frames to MainWindow after a burst has been armed.
    void deliverFrames(MainWindow &window, int w, int h, int frames, int baseValue);

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

    deliverFrames(window, w, h, frames, baseValue);

    auto *view = window.findChild<ImageViewWidget *>();
    return view ? view->image() : QImage();
}

void TestDarkFrameAcquisition::deliverFrames(MainWindow &window, int w, int h,
                                             int frames, int baseValue)
{
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
    // row stride wider than width*2. The burst must both survive and produce
    // the right pixel values; a linear constBits() walk used to shift every
    // row after the first and write the wrong values back.
    for (int width : {64, 63, 641}) {
        MainWindow window;
        window.resize(1130, 870);
        window.show();
        QCoreApplication::processEvents();

        const QImage shown = runBurst(window, width, 48, 5, 500);
        QVERIFY2(!shown.isNull(),
                 qPrintable(QStringLiteral("no frame for width %1").arg(width)));
        QCOMPARE(shown.width(), width);

        // Frames hold 500..504, so the mean is 502.
        for (int y = 0; y < shown.height(); ++y) {
            const auto *row = reinterpret_cast<const quint16 *>(shown.constScanLine(y));
            for (int x = 0; x < shown.width(); ++x) {
                QCOMPARE(int(row[x]), 502);
            }
        }
    }
}

// 请求一个远超上限的帧数，MainWindow 必须按 CalibrationDialog::kMaxFrameCount
// 夹住，状态栏上写出来的就是夹取之后的帧数。
//
// 注意这条只锁住「行为」，锁不住「数字的出处」：曾把 MainWindow 里的常量换回
// 字面量 1000 重跑，因为常量本身也是 1000，这条照样通过。真正防漂移的是两边
// 共用同一个常量这一件事本身。
void TestDarkFrameAcquisition::test_burst_request_above_the_limit_is_clamped()
{
    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window, "onAcquireDarkFrameStartRequested",
                                      Qt::DirectConnection, Q_ARG(int, 999999)));

    const QString message = window.statusBar()->currentMessage();
    QVERIFY2(message.contains(QStringLiteral("/%1").arg(CalibrationDialog::kMaxFrameCount)),
             qPrintable(QStringLiteral("状态栏没写夹取后的帧数：%1").arg(message)));
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

    auto *pathField = dialog->findChild<QLineEdit *>(QStringLiteral("matchedDarkPathEdit"));
    auto *enableCheck = dialog->findChild<QCheckBox *>(QStringLiteral("darkEnableCheckBox"));
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

// A non-dark Acquire button goes through the dialog's kind/exposure getters.
// The status refresh that follows the burst must not overwrite the exposure the
// user typed: it is still needed for the next acquisition and for the apply.
void TestDarkFrameAcquisition::test_flat_acquire_uses_dialog_slot_and_exposure()
{
    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();

    auto *dialog = openCalibrationDialog(window);
    QVERIFY(dialog);

    auto *exposureSpin = dialog->findChild<QDoubleSpinBox *>(QStringLiteral("flatExposureSpin"));
    auto *acquire = dialog->findChild<QPushButton *>(QStringLiteral("flatAcquireButton"));
    auto *pathField = dialog->findChild<QLineEdit *>(QStringLiteral("flatPathEdit"));
    QVERIFY(exposureSpin);
    QVERIFY(acquire);
    QVERIFY(pathField);

    // No camera is connected in this test; the dialog's own Acquire gate is
    // opened explicitly to exercise the signal path.
    dialog->setAcquireEnabled(true);
    dialog->setFrameCount(3);
    exposureSpin->setValue(2.5);
    acquire->click();
    QCoreApplication::processEvents();

    deliverFrames(window, 32, 24, 3, 200);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QVERIFY2(pathField->placeholderText().contains(QStringLiteral("in-memory flat frame")),
             qPrintable(pathField->placeholderText()));
    QVERIFY2(pathField->placeholderText().contains(QStringLiteral("3-frame average")),
             qPrintable(pathField->placeholderText()));
    QVERIFY(pathField->text().isEmpty());
    QCOMPARE(exposureSpin->value(), 2.5);
}

// The whole point of Save As is that the in-memory average survives the
// session. Drive the real burst, write it through the same FileSaverWorker the
// File menu uses, and check both the pixels and the metadata sidecar.
void TestDarkFrameAcquisition::test_acquired_reference_can_be_saved_to_disk()
{
    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();

    QTemporaryDir dir;
    QVERIFY2(dir.isValid(), "无法创建临时目录");

    constexpr int kFrames = 4;
    const QImage shown = runBurst(window, 4, 2, kFrames, 100);
    QVERIFY(!shown.isNull());

    const QString path = dir.filePath(QStringLiteral("dark.csv"));
    QVERIFY(QMetaObject::invokeMethod(&window, "saveReferenceFrameToFile",
                                      Qt::DirectConnection,
                                      Q_ARG(int, static_cast<int>(
                                                     Calibration::ReferenceKind::MatchedDark)),
                                      Q_ARG(QString, path)));

    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(path), 5000);

    const QString metadataPath = dir.filePath(QStringLiteral("dark_metadata.json"));
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(metadataPath), 5000);

    // The sidecar is written after the image, so both files are complete now.
    QFile imageFile(path);
    QVERIFY2(imageFile.open(QIODevice::ReadOnly), "CSV 没有写出来");
    const QString content = QString::fromUtf8(imageFile.readAll());
    imageFile.close();

    // Frames hold 100..103, so a 4-frame average is 101.5 -> 102.
    QVERIFY2(content.contains(QStringLiteral("102")), qPrintable(content));
    QVERIFY2(!content.contains(QStringLiteral("103")), qPrintable(content));

    QFile metadataFile(metadataPath);
    QVERIFY(metadataFile.open(QIODevice::ReadOnly));
    const QString metadata = QString::fromUtf8(metadataFile.readAll());
    QVERIFY2(metadata.contains(QStringLiteral("calibrationReference")),
             qPrintable(metadata));
    QVERIFY2(metadata.contains(QStringLiteral("averagedFrames")), qPrintable(metadata));
}

QTEST_MAIN(TestDarkFrameAcquisition)
#include "test_dark_frame_acquisition.moc"
