#include "DespikeDialog.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSignalBlocker>
#include <QSpinBox>

namespace {
// 主/次阈值的量程。主阈值低于 1 会把噪声全当尖峰，高于 20 基本什么都抓不到；
// 次阈值 0 表示关闭那条判据，所以从 0 起
constexpr double kThresholdMin = 0.5;
constexpr double kThresholdMax = 20.0;
// 均值窗口半宽上限比谱线宽度大得多就没有意义了，200 已经覆盖 6000 点谱线的
// 常用范围，再大只会让一次处理的代价变得很高
constexpr int kWindowHalfWidthMax = 200;
} // namespace

DespikeDialog::DespikeDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Despike");
    setMinimumWidth(300);
    setModal(false);

    setupUi();
}

DespikeDialog::~DespikeDialog() = default;

void DespikeDialog::setupUi()
{
    QFormLayout *layout = new QFormLayout(this);

    const PostProcess::DespikeParams defaults;

    m_primarySpin = new QDoubleSpinBox(this);
    m_primarySpin->setObjectName("primaryThresholdSpin");
    m_primarySpin->setDecimals(1);
    m_primarySpin->setSingleStep(0.1);
    m_primarySpin->setRange(kThresholdMin, kThresholdMax);
    m_primarySpin->setValue(defaults.primaryThreshold);
    m_primarySpin->setToolTip(
        "Primary threshold: |modified Z| of the first difference. "
        "Points above it become spike candidates.");
    layout->addRow("Primary threshold (Z):", m_primarySpin);

    m_secondarySpin = new QDoubleSpinBox(this);
    m_secondarySpin->setObjectName("secondaryThresholdSpin");
    m_secondarySpin->setDecimals(1);
    m_secondarySpin->setSingleStep(0.1);
    m_secondarySpin->setRange(0.0, kThresholdMax);
    m_secondarySpin->setValue(defaults.secondaryThreshold);
    m_secondarySpin->setToolTip(
        "Secondary threshold: the two differences just outside a spike must stay "
        "below it, which keeps spectral bands from being taken for spikes. "
        "0 disables this check.");
    layout->addRow("Secondary threshold (Z):", m_secondarySpin);

    m_windowSpin = new QSpinBox(this);
    m_windowSpin->setObjectName("windowHalfWidthSpin");
    m_windowSpin->setRange(1, kWindowHalfWidthMax);
    m_windowSpin->setValue(defaults.windowHalfWidth);
    m_windowSpin->setToolTip(
        "Mean window half-width: a replacement is the mean of the unmarked "
        "points within +/- this many points.");
    layout->addRow("Mean window half-width:", m_windowSpin);

    connect(m_primarySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { emit paramsChanged(primaryThreshold(),
                                                      secondaryThreshold(),
                                                      windowHalfWidth()); });
    connect(m_secondarySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { emit paramsChanged(primaryThreshold(),
                                                      secondaryThreshold(),
                                                      windowHalfWidth()); });
    connect(m_windowSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { emit paramsChanged(primaryThreshold(),
                                                   secondaryThreshold(),
                                                   windowHalfWidth()); });
}

PostProcess::DespikeParams DespikeDialog::despikeParams() const
{
    // passes 不在窗口上，用算法默认的两遍
    PostProcess::DespikeParams params;
    params.primaryThreshold = primaryThreshold();
    params.secondaryThreshold = secondaryThreshold();
    params.windowHalfWidth = windowHalfWidth();
    return params;
}

void DespikeDialog::setDespikeParams(const PostProcess::DespikeParams &params)
{
    // 控件值是代码写进去的，不是用户输入，不能反手再发一次 paramsChanged
    const QSignalBlocker primaryBlocker(m_primarySpin);
    const QSignalBlocker secondaryBlocker(m_secondarySpin);
    const QSignalBlocker windowBlocker(m_windowSpin);

    m_primarySpin->setValue(params.primaryThreshold);
    m_secondarySpin->setValue(params.secondaryThreshold);
    m_windowSpin->setValue(params.windowHalfWidth);
}

double DespikeDialog::primaryThreshold() const
{
    return m_primarySpin->value();
}

double DespikeDialog::secondaryThreshold() const
{
    return m_secondarySpin->value();
}

int DespikeDialog::windowHalfWidth() const
{
    return m_windowSpin->value();
}
