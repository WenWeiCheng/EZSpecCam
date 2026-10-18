#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QObject>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTest>
#include <QVector>
#include <cmath>

#include "display/SpectrumViewWidget.h"
#include "widgets/dialogs/DespikeDialog.h"

namespace {

// 与 test_despike 里的同一套确定性噪声：均匀噪声的一次差分 |Z| 最大约 2.3，
// 不会自己越过去尖峰的阈值
QVector<double> noisyBaseline(int size, double level, double amplitude, unsigned seed)
{
    QVector<double> y(size);
    unsigned state = seed;
    for (int i = 0; i < size; ++i) {
        state = state * 1664525u + 1013904223u;
        const double unit = static_cast<double>(state >> 8) / 16777216.0;
        y[i] = level + (unit - 0.5) * 2.0 * amplitude;
    }
    return y;
}

QVector<double> spikeSpectrum(unsigned seed, int spikeIndex)
{
    QVector<double> y = noisyBaseline(256, 1000.0, 20.0, seed);
    y[spikeIndex] += 4000.0;
    return y;
}

QVector<double> indices(int size)
{
    QVector<double> x(size);
    for (int i = 0; i < size; ++i) {
        x[i] = i;
    }
    return x;
}

QDoubleSpinBox *doubleSpin(DespikeDialog &dialog, const char *name)
{
    return dialog.findChild<QDoubleSpinBox *>(QLatin1String(name));
}

QSpinBox *intSpin(DespikeDialog &dialog, const char *name)
{
    return dialog.findChild<QSpinBox *>(QLatin1String(name));
}

} // namespace

class TestDespikeDialog : public QObject
{
    Q_OBJECT

private slots:
    void test_window_exposes_only_three_parameters();
    void test_defaults_match_algorithm_defaults();
    void test_edit_emits_all_three_params();
    void test_secondary_threshold_zero_is_allowed();
    void test_window_half_width_cannot_go_below_one();
    void test_set_params_does_not_emit();
    void test_edit_reaches_spectrum_view();
};

void TestDespikeDialog::test_window_exposes_only_three_parameters()
{
    DespikeDialog dialog;

    // 去尖峰只暴露主阈值、次阈值、均值窗口半宽三个参数
    QCOMPARE(dialog.findChildren<QAbstractSpinBox *>().size(), 3);
    QVERIFY(dialog.findChildren<QComboBox *>().isEmpty());
    QVERIFY(dialog.findChildren<QCheckBox *>().isEmpty());
}

void TestDespikeDialog::test_defaults_match_algorithm_defaults()
{
    DespikeDialog dialog;
    const PostProcess::DespikeParams defaults;

    QCOMPARE(dialog.primaryThreshold(), defaults.primaryThreshold);
    QCOMPARE(dialog.secondaryThreshold(), defaults.secondaryThreshold);
    QCOMPARE(dialog.windowHalfWidth(), defaults.windowHalfWidth);
    // 窗口上没有 passes，算法默认的两遍原样保留
    QCOMPARE(dialog.despikeParams().passes, defaults.passes);
}

void TestDespikeDialog::test_edit_emits_all_three_params()
{
    DespikeDialog dialog;
    QSignalSpy spy(&dialog, &DespikeDialog::paramsChanged);

    doubleSpin(dialog, "primaryThresholdSpin")->setValue(4.5);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toDouble(), 4.5);
    QCOMPARE(spy.first().at(1).toDouble(), 2.5);
    QCOMPARE(spy.first().at(2).toInt(), 5);

    intSpin(dialog, "windowHalfWidthSpin")->setValue(9);

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.last().at(0).toDouble(), 4.5);
    QCOMPARE(spy.last().at(1).toDouble(), 2.5);
    QCOMPARE(spy.last().at(2).toInt(), 9);

    const PostProcess::DespikeParams params = dialog.despikeParams();
    QCOMPARE(params.primaryThreshold, 4.5);
    QCOMPARE(params.secondaryThreshold, 2.5);
    QCOMPARE(params.windowHalfWidth, 9);
}

void TestDespikeDialog::test_secondary_threshold_zero_is_allowed()
{
    DespikeDialog dialog;

    // 0 是「关闭次阈值判据」，必须能设进去，不能被下限夹到别的值
    doubleSpin(dialog, "secondaryThresholdSpin")->setValue(0.0);

    QCOMPARE(dialog.secondaryThreshold(), 0.0);
    QCOMPARE(dialog.despikeParams().secondaryThreshold, 0.0);
}

void TestDespikeDialog::test_window_half_width_cannot_go_below_one()
{
    DespikeDialog dialog;

    intSpin(dialog, "windowHalfWidthSpin")->setValue(0);

    // 半宽 0 会让「邻域均值」退化成这一个点本身，等于没处理
    QCOMPARE(dialog.windowHalfWidth(), 1);
}

void TestDespikeDialog::test_set_params_does_not_emit()
{
    DespikeDialog dialog;
    QSignalSpy spy(&dialog, &DespikeDialog::paramsChanged);

    PostProcess::DespikeParams params;
    params.primaryThreshold = 5.0;
    params.secondaryThreshold = 0.0;
    params.windowHalfWidth = 9;
    dialog.setDespikeParams(params);

    // 写回控件是代码行为，不是用户输入，不能再发一遍信号
    QCOMPARE(spy.count(), 0);
    QCOMPARE(dialog.primaryThreshold(), 5.0);
    QCOMPARE(dialog.secondaryThreshold(), 0.0);
    QCOMPARE(dialog.windowHalfWidth(), 9);
}

void TestDespikeDialog::test_edit_reaches_spectrum_view()
{
    constexpr int spikeIndex = 50;

    DespikeDialog dialog;
    SpectrumViewWidget view;

    // 和 MainWindow 里一样的接法：对话框改了参数就写到光谱控件上
    QObject::connect(&dialog, &DespikeDialog::paramsChanged, &view,
                     [&dialog, &view](double, double, int) {
                         view.setDespikeParams(dialog.despikeParams());
                     });

    const QVector<double> y = spikeSpectrum(31u, spikeIndex);
    view.setData(indices(y.size()), y);

    // 去尖峰没开时显示的就是原始谱
    QCOMPARE(view.yData(), y);
    QCOMPARE(view.despikedPointCount(), 0);

    view.setDespikeEnabled(true);
    QCOMPARE(view.despikedPointCount(), 1);
    const double defaultWindowValue = view.yData()[spikeIndex];

    // 半宽改成 1：替换值变成左右相邻两点的均值，和默认的 ±5 窗口不同
    intSpin(dialog, "windowHalfWidthSpin")->setValue(1);
    QCOMPARE(view.despikeParams().windowHalfWidth, 1);

    const double expected = 0.5 * (y[spikeIndex - 1] + y[spikeIndex + 1]);
    QVERIFY2(std::abs(view.yData()[spikeIndex] - expected) < 1e-6,
             qPrintable(QStringLiteral("replacement %1 != expected %2")
                            .arg(view.yData()[spikeIndex])
                            .arg(expected)));
    QVERIFY(std::abs(view.yData()[spikeIndex] - defaultWindowValue) > 1e-6);

    // 关掉后立刻回到原始谱，缓存的是未处理的数据而不是处理过的
    view.setDespikeEnabled(false);
    QCOMPARE(view.yData()[spikeIndex], y[spikeIndex]);
    QCOMPARE(view.despikedPointCount(), 0);
}

QTEST_MAIN(TestDespikeDialog)

#include "test_despike_dialog.moc"
