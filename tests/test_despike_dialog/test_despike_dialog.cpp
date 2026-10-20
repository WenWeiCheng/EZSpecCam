#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QObject>
#include <QSignalSpy>
#include <QTest>

#include "widgets/dialogs/DespikeDialog.h"

namespace {

QDoubleSpinBox *primarySpin(DespikeDialog &dialog)
{
    return dialog.findChild<QDoubleSpinBox *>(QStringLiteral("primaryThresholdSpin"));
}

} // namespace

class TestDespikeDialog : public QObject
{
    Q_OBJECT

private slots:
    void test_window_exposes_only_primary_threshold();
    void test_defaults_match_algorithm_defaults();
    void test_edit_emits_params_changed();
    void test_threshold_cannot_go_below_minimum();
    void test_set_params_does_not_emit();
};

void TestDespikeDialog::test_window_exposes_only_primary_threshold()
{
    DespikeDialog dialog;

    // 现在只剩主阈值一个参数（次阈值、均值窗口、迭代遍数都已移除）
    QCOMPARE(dialog.findChildren<QAbstractSpinBox *>().size(), 1);
    QVERIFY(dialog.findChildren<QComboBox *>().isEmpty());
    QVERIFY(primarySpin(dialog) != nullptr);
}

void TestDespikeDialog::test_defaults_match_algorithm_defaults()
{
    DespikeDialog dialog;
    const PostProcess::DespikeParams defaults;

    QCOMPARE(dialog.primaryThreshold(), defaults.primaryThreshold);
    QCOMPARE(dialog.despikeParams().primaryThreshold, defaults.primaryThreshold);
}

void TestDespikeDialog::test_edit_emits_params_changed()
{
    DespikeDialog dialog;
    QSignalSpy spy(&dialog, &DespikeDialog::paramsChanged);

    primarySpin(dialog)->setValue(4.5);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(dialog.primaryThreshold(), 4.5);
    QCOMPARE(dialog.despikeParams().primaryThreshold, 4.5);
}

void TestDespikeDialog::test_threshold_cannot_go_below_minimum()
{
    DespikeDialog dialog;

    primarySpin(dialog)->setValue(0.0);

    // 低于 1 会把噪声全当尖峰，下限必须兜住
    QCOMPARE(dialog.primaryThreshold(), 0.5);
}

void TestDespikeDialog::test_set_params_does_not_emit()
{
    DespikeDialog dialog;
    QSignalSpy spy(&dialog, &DespikeDialog::paramsChanged);

    PostProcess::DespikeParams params;
    params.primaryThreshold = 5.0;
    dialog.setDespikeParams(params);

    // 写回控件是代码行为，不是用户输入，不能再发一遍信号
    QCOMPARE(spy.count(), 0);
    QCOMPARE(dialog.primaryThreshold(), 5.0);
}

QTEST_MAIN(TestDespikeDialog)

#include "test_despike_dialog.moc"
