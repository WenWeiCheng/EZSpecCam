// CalibrationDialog also owns the dark-frame acquisition controls (the
// "Frames to average" row), so the burst can be started and the Enable
// switch flipped without leaving the window. These cover the widget's own
// state machine: the Acquire button must lock while a burst is running, must
// stay locked while no camera is connected, and the in-memory marker must
// never leak into the persisted path.
#include <QCheckBox>
#include <QCoreApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTest>

#include "widgets/dialogs/CalibrationDialog.h"

static QPushButton *acquireButtonOf(CalibrationDialog *dialog)
{
    return dialog->findChild<QPushButton *>(QStringLiteral("acquireButton"));
}

static QSpinBox *frameCountSpinOf(CalibrationDialog *dialog)
{
    return dialog->findChild<QSpinBox *>(QStringLiteral("frameCountSpinBox"));
}

class TestCalibrationDialog : public QObject
{
    Q_OBJECT

private slots:
    void init() {}
    void cleanup() {}

    void test_acquire_emits_requested_frame_count();
    void test_acquire_locks_while_running();
    void test_acquire_disabled_without_camera();
    void test_frame_count_locked_while_running();
    void test_in_memory_marker_never_becomes_the_path();
    void test_applied_reports_enable_and_bias();
    void test_in_memory_placeholder_is_not_truncated();
    void test_frame_count_cannot_exceed_the_shared_limit();
};

void TestCalibrationDialog::test_acquire_emits_requested_frame_count()
{
    CalibrationDialog dialog;
    auto *acquire = acquireButtonOf(&dialog);
    QVERIFY2(acquire, "对话框里没有 Acquire 按钮");

    auto *spin = frameCountSpinOf(&dialog);
    QVERIFY(spin);
    spin->setValue(7);

    QSignalSpy spy(&dialog, &CalibrationDialog::acquireRequested);
    acquire->click();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 7);
}

void TestCalibrationDialog::test_acquire_locks_while_running()
{
    CalibrationDialog dialog;
    auto *acquire = acquireButtonOf(&dialog);
    QVERIFY(acquire);

    QSignalSpy spy(&dialog, &CalibrationDialog::acquireRequested);
    acquire->click();
    QCOMPARE(spy.count(), 1);

    // 已经在采了，再点一次不能把第二个 burst 排上队
    QVERIFY(!acquire->isEnabled());
    acquire->click();
    QCOMPARE(spy.count(), 1);

    dialog.setAcquireInProgress(false);
    QVERIFY(acquire->isEnabled());
    acquire->click();
    QCOMPARE(spy.count(), 2);
}

void TestCalibrationDialog::test_acquire_disabled_without_camera()
{
    CalibrationDialog dialog;
    auto *acquire = acquireButtonOf(&dialog);
    QVERIFY(acquire);

    QSignalSpy spy(&dialog, &CalibrationDialog::acquireRequested);

    dialog.setAcquireEnabled(false);
    QVERIFY(!acquire->isEnabled());
    acquire->click();
    QCOMPARE(spy.count(), 0);

    // 「相机没连上」和「正在采集」是两条独立的限制：先因为没连上而禁用，
    // 再进采集态，最后重新允许采集 —— 结束时不能停在禁用上
    dialog.setAcquireInProgress(true);
    dialog.setAcquireEnabled(true);
    dialog.setAcquireInProgress(false);
    QVERIFY(acquire->isEnabled());
    acquire->click();
    QCOMPARE(spy.count(), 1);
}

void TestCalibrationDialog::test_frame_count_locked_while_running()
{
    CalibrationDialog dialog;
    auto *spin = frameCountSpinOf(&dialog);
    QVERIFY(spin);

    dialog.setAcquireInProgress(true);
    QVERIFY(!spin->isEnabled());

    dialog.setAcquireInProgress(false);
    QVERIFY(spin->isEnabled());
}

void TestCalibrationDialog::test_in_memory_marker_never_becomes_the_path()
{
    CalibrationDialog dialog;
    auto *path = dialog.findChild<QLineEdit *>();
    QVERIFY(path);

    dialog.setInMemoryDarkFrameUsed(true, 10);
    QVERIFY(path->text().isEmpty());
    QVERIFY2(path->placeholderText().contains(QStringLiteral("in-memory dark frame")),
             qPrintable(path->placeholderText()));
    QVERIFY(path->placeholderText().contains(QStringLiteral("10-frame")));

    // 用户后来选了文件，指示文字必须让位给真实路径
    dialog.setDarkFramePath(QStringLiteral("/tmp/dark.tif"));
    QCOMPARE(path->text(), QStringLiteral("/tmp/dark.tif"));
    QCOMPARE(dialog.darkFramePath(), QStringLiteral("/tmp/dark.tif"));
}

void TestCalibrationDialog::test_applied_reports_enable_and_bias()
{
    CalibrationDialog dialog;
    auto *enable = dialog.findChild<QCheckBox *>();
    QVERIFY(enable);
    auto *ok = dialog.findChild<QPushButton *>(QStringLiteral("okButton"));
    QVERIFY(ok);

    enable->setChecked(true);
    dialog.setCustomBias(250);

    QSignalSpy spy(&dialog, &CalibrationDialog::applied);
    ok->click();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
    QCOMPARE(spy.at(0).at(1).toString(), QString());
    QCOMPARE(spy.at(0).at(2).toInt(), 250);
}

// 「in-memory dark frame, N-frame average」是采完暗帧之后用户第一眼要读的
// 一行，QLineEdit 一旦放不下就会截成「(in-memory dark frame, …」，而窗口宽度
// 又跟着系统字体走，写死像素换个平台就对不上。这里按当前字体直接量：提示的
// 宽度必须能整个放进输入框。
void TestCalibrationDialog::test_in_memory_placeholder_is_not_truncated()
{
    CalibrationDialog dialog;

    auto *path = dialog.findChild<QLineEdit *>();
    QVERIFY(path);
    auto *spin = frameCountSpinOf(&dialog);
    QVERIFY(spin);

    // 用上限去试：位数最多的那条提示才是最窄的窗口也要放得下的一条
    dialog.setInMemoryDarkFrameUsed(true, spin->maximum());
    dialog.show();
    QCoreApplication::processEvents();

    // 16 和 CalibrationDialog.cpp 里的 kPlaceholderSlack 是同一个数：
    // 留给 QLineEdit 边框和左右内边距的宽度
    const int available = path->width() - 16;
    const int needed = path->fontMetrics().horizontalAdvance(path->placeholderText());
    QVERIFY2(needed <= available,
             qPrintable(QStringLiteral("提示被截断：需要 %1px，输入框只有 %2px（%3）")
                            .arg(needed).arg(available).arg(path->placeholderText())));
}

// MainWindow::onAcquireDarkFrameStartRequested 按 CalibrationDialog::kMaxFrameCount
// 夹取请求值。对话框要是能吐出更大的数，用户填 5000、实际采 1000，界面上却
// 一点提示都没有 —— 所以输入框的范围必须就是那个上限。
void TestCalibrationDialog::test_frame_count_cannot_exceed_the_shared_limit()
{
    CalibrationDialog dialog;
    auto *spin = frameCountSpinOf(&dialog);
    QVERIFY(spin);

    QCOMPARE(spin->minimum(), 1);
    QCOMPARE(spin->maximum(), CalibrationDialog::kMaxFrameCount);

    spin->setValue(999999);
    QCOMPARE(spin->value(), CalibrationDialog::kMaxFrameCount);

    spin->setValue(0);
    QCOMPARE(spin->value(), 1);
    QCOMPARE(dialog.frameCount(), 1);
}

QTEST_MAIN(TestCalibrationDialog)
#include "test_calibration_dialog.moc"