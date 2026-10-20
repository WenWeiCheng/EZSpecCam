#include "DespikeDialog.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSignalBlocker>

namespace {
// 主阈值的量程。低于 1 会把噪声全当尖峰，高于 20 基本什么都抓不到
constexpr double kThresholdMin = 0.5;
constexpr double kThresholdMax = 20.0;
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
        "Primary threshold: an isolated pixel must be at least this many noise "
        "sigmas brighter than all 8 neighbours before it is replaced.");
    layout->addRow("Primary threshold (Z):", m_primarySpin);

    connect(m_primarySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { emit paramsChanged(); });
}

PostProcess::DespikeParams DespikeDialog::despikeParams() const
{
    PostProcess::DespikeParams params;
    params.primaryThreshold = primaryThreshold();
    return params;
}

void DespikeDialog::setDespikeParams(const PostProcess::DespikeParams &params)
{
    // 控件值是代码写进去的，不是用户输入，不能反手再发一次 paramsChanged
    const QSignalBlocker blocker(m_primarySpin);
    m_primarySpin->setValue(params.primaryThreshold);
}

double DespikeDialog::primaryThreshold() const
{
    return m_primarySpin->value();
}
