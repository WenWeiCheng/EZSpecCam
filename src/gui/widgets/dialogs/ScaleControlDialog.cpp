#include "ScaleControlDialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDoubleSpinBox>

ScaleControlDialog::ScaleControlDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Scale Control");
    setModal(false);
    setMinimumWidth(300);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    createImageGroup();
    createSpectrumGroup();

    mainLayout->addWidget(m_imageGroup);
    mainLayout->addWidget(m_spectrumGroup);
    mainLayout->addStretch();
}

ScaleControlDialog::~ScaleControlDialog()
{
}

void ScaleControlDialog::createImageGroup()
{
    m_imageGroup = new QGroupBox("Image Scale", this);

    QFormLayout *layout = new QFormLayout(m_imageGroup);

    m_imageScaleTypeCombo = new QComboBox(this);
    m_imageScaleTypeCombo->addItem("linear", 0);
    m_imageScaleTypeCombo->addItem("log", 1);
    layout->addRow("Type:", m_imageScaleTypeCombo);

    m_imageColorScaleModeCombo = new QComboBox(this);
    m_imageColorScaleModeCombo->addItem("Auto (min-max)", 0);
    m_imageColorScaleModeCombo->addItem("8-bit (0-255)", 1);
    m_imageColorScaleModeCombo->addItem("16-bit (0-65535)", 2);
    layout->addRow("Range:", m_imageColorScaleModeCombo);

    connect(m_imageScaleTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScaleControlDialog::onImageScaleTypeChanged);
    connect(m_imageColorScaleModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScaleControlDialog::onImageColorScaleModeChanged);
}

void ScaleControlDialog::createSpectrumGroup()
{
    m_spectrumGroup = new QGroupBox("Spectrum Scale", this);

    QFormLayout *layout = new QFormLayout(m_spectrumGroup);

    m_spectrumScaleTypeCombo = new QComboBox(this);
    m_spectrumScaleTypeCombo->addItem("linear", 0);
    m_spectrumScaleTypeCombo->addItem("log", 1);
    layout->addRow("Type:", m_spectrumScaleTypeCombo);

    m_spectrumXRangeModeCombo = new QComboBox(this);
    m_spectrumXRangeModeCombo->addItem("Auto", 0);
    m_spectrumXRangeModeCombo->addItem("Manual", 1);
    layout->addRow("X Range:", m_spectrumXRangeModeCombo);

    m_spectrumManualXMinSpin = new QDoubleSpinBox(this);
    m_spectrumManualXMinSpin->setDecimals(2);
    m_spectrumManualXMinSpin->setRange(-1000000.0, 1000000.0);
    m_spectrumManualXMinSpin->setValue(0.0);

    m_spectrumManualXMaxSpin = new QDoubleSpinBox(this);
    m_spectrumManualXMaxSpin->setDecimals(2);
    m_spectrumManualXMaxSpin->setRange(-1000000.0, 1000000.0);
    m_spectrumManualXMaxSpin->setValue(100.0);

    auto *xManualLayout = new QHBoxLayout();
    xManualLayout->addWidget(m_spectrumManualXMinSpin);
    xManualLayout->addWidget(m_spectrumManualXMaxSpin);
    layout->addRow("X Min / Max:", xManualLayout);

    m_spectrumYRangeModeCombo = new QComboBox(this);
    m_spectrumYRangeModeCombo->addItem("Auto", 0);
    m_spectrumYRangeModeCombo->addItem("Manual", 1);
    layout->addRow("Y Range:", m_spectrumYRangeModeCombo);

    m_spectrumManualYMinSpin = new QDoubleSpinBox(this);
    m_spectrumManualYMinSpin->setDecimals(2);
    m_spectrumManualYMinSpin->setRange(-1000000.0, 1000000.0);
    m_spectrumManualYMinSpin->setValue(0.0);

    m_spectrumManualYMaxSpin = new QDoubleSpinBox(this);
    m_spectrumManualYMaxSpin->setDecimals(2);
    m_spectrumManualYMaxSpin->setRange(-1000000.0, 1000000.0);
    m_spectrumManualYMaxSpin->setValue(100.0);

    auto *yManualLayout = new QHBoxLayout();
    yManualLayout->addWidget(m_spectrumManualYMinSpin);
    yManualLayout->addWidget(m_spectrumManualYMaxSpin);
    layout->addRow("Y Min / Max:", yManualLayout);

    connect(m_spectrumScaleTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScaleControlDialog::onSpectrumScaleTypeChanged);
    connect(m_spectrumXRangeModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScaleControlDialog::onSpectrumXRangeModeChanged);
    connect(m_spectrumYRangeModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScaleControlDialog::onSpectrumYRangeModeChanged);

    connect(m_spectrumManualXMinSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { onSpectrumManualXMinChanged(); });
    connect(m_spectrumManualXMaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { onSpectrumManualXMaxChanged(); });
    connect(m_spectrumManualYMinSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { onSpectrumManualYMinChanged(); });
    connect(m_spectrumManualYMaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { onSpectrumManualYMaxChanged(); });

    updateSpectrumManualXEnabled();
    updateSpectrumManualYEnabled();
}

void ScaleControlDialog::updateSpectrumManualXEnabled()
{
    bool manual = m_spectrumXRangeModeCombo->currentData().toInt() == 1;
    m_spectrumManualXMinSpin->setEnabled(manual);
    m_spectrumManualXMaxSpin->setEnabled(manual);
}

void ScaleControlDialog::updateSpectrumManualYEnabled()
{
    bool manual = m_spectrumYRangeModeCombo->currentData().toInt() == 1;
    m_spectrumManualYMinSpin->setEnabled(manual);
    m_spectrumManualYMaxSpin->setEnabled(manual);
}

void ScaleControlDialog::setImageScaleType(int type)
{
    int index = m_imageScaleTypeCombo->findData(type);
    if (index >= 0) {
        m_imageScaleTypeCombo->blockSignals(true);
        m_imageScaleTypeCombo->setCurrentIndex(index);
        m_imageScaleTypeCombo->blockSignals(false);
    }
}

void ScaleControlDialog::setImageColorScaleMode(int mode)
{
    int index = m_imageColorScaleModeCombo->findData(mode);
    if (index >= 0) {
        m_imageColorScaleModeCombo->blockSignals(true);
        m_imageColorScaleModeCombo->setCurrentIndex(index);
        m_imageColorScaleModeCombo->blockSignals(false);
    }
}

void ScaleControlDialog::setSpectrumScaleType(int type)
{
    int index = m_spectrumScaleTypeCombo->findData(type);
    if (index >= 0) {
        m_spectrumScaleTypeCombo->blockSignals(true);
        m_spectrumScaleTypeCombo->setCurrentIndex(index);
        m_spectrumScaleTypeCombo->blockSignals(false);
    }
}

void ScaleControlDialog::setSpectrumXRangeMode(int mode)
{
    int index = m_spectrumXRangeModeCombo->findData(mode);
    if (index >= 0) {
        m_spectrumXRangeModeCombo->blockSignals(true);
        m_spectrumXRangeModeCombo->setCurrentIndex(index);
        m_spectrumXRangeModeCombo->blockSignals(false);
        updateSpectrumManualXEnabled();
    }
}

void ScaleControlDialog::setSpectrumYRangeMode(int mode)
{
    int index = m_spectrumYRangeModeCombo->findData(mode);
    if (index >= 0) {
        m_spectrumYRangeModeCombo->blockSignals(true);
        m_spectrumYRangeModeCombo->setCurrentIndex(index);
        m_spectrumYRangeModeCombo->blockSignals(false);
        updateSpectrumManualYEnabled();
    }
}

void ScaleControlDialog::setSpectrumManualXRange(double min, double max)
{
    m_spectrumManualXMinSpin->blockSignals(true);
    m_spectrumManualXMaxSpin->blockSignals(true);
    m_spectrumManualXMinSpin->setValue(min);
    m_spectrumManualXMaxSpin->setValue(max);
    m_spectrumManualXMinSpin->blockSignals(false);
    m_spectrumManualXMaxSpin->blockSignals(false);
}

void ScaleControlDialog::setSpectrumManualYRange(double min, double max)
{
    m_spectrumManualYMinSpin->blockSignals(true);
    m_spectrumManualYMaxSpin->blockSignals(true);
    m_spectrumManualYMinSpin->setValue(min);
    m_spectrumManualYMaxSpin->setValue(max);
    m_spectrumManualYMinSpin->blockSignals(false);
    m_spectrumManualYMaxSpin->blockSignals(false);
}

int ScaleControlDialog::imageScaleType() const
{
    return m_imageScaleTypeCombo->currentData().toInt();
}

int ScaleControlDialog::imageColorScaleMode() const
{
    return m_imageColorScaleModeCombo->currentData().toInt();
}

int ScaleControlDialog::spectrumScaleType() const
{
    return m_spectrumScaleTypeCombo->currentData().toInt();
}

int ScaleControlDialog::spectrumXRangeMode() const
{
    return m_spectrumXRangeModeCombo->currentData().toInt();
}

int ScaleControlDialog::spectrumYRangeMode() const
{
    return m_spectrumYRangeModeCombo->currentData().toInt();
}

double ScaleControlDialog::spectrumManualXMin() const
{
    return m_spectrumManualXMinSpin->value();
}

double ScaleControlDialog::spectrumManualXMax() const
{
    return m_spectrumManualXMaxSpin->value();
}

double ScaleControlDialog::spectrumManualYMin() const
{
    return m_spectrumManualYMinSpin->value();
}

double ScaleControlDialog::spectrumManualYMax() const
{
    return m_spectrumManualYMaxSpin->value();
}

void ScaleControlDialog::onImageScaleTypeChanged(int index)
{
    Q_UNUSED(index)
    emit imageScaleTypeChanged(imageScaleType());
}

void ScaleControlDialog::onImageColorScaleModeChanged(int index)
{
    Q_UNUSED(index)
    emit imageColorScaleModeChanged(imageColorScaleMode());
}

void ScaleControlDialog::onSpectrumScaleTypeChanged(int index)
{
    Q_UNUSED(index)
    emit spectrumScaleTypeChanged(spectrumScaleType());
}

void ScaleControlDialog::onSpectrumXRangeModeChanged(int index)
{
    Q_UNUSED(index)
    updateSpectrumManualXEnabled();
    emit spectrumXRangeModeChanged(spectrumXRangeMode());
    if (spectrumXRangeMode() == 1) {
        emit spectrumManualXRangeChanged(spectrumManualXMin(), spectrumManualXMax());
    }
}

void ScaleControlDialog::onSpectrumYRangeModeChanged(int index)
{
    Q_UNUSED(index)
    updateSpectrumManualYEnabled();
    emit spectrumYRangeModeChanged(spectrumYRangeMode());
    if (spectrumYRangeMode() == 1) {
        emit spectrumManualYRangeChanged(spectrumManualYMin(), spectrumManualYMax());
    }
}

void ScaleControlDialog::onSpectrumManualXMinChanged()
{
    if (spectrumXRangeMode() == 1) {
        emit spectrumManualXRangeChanged(spectrumManualXMin(), spectrumManualXMax());
    }
}

void ScaleControlDialog::onSpectrumManualXMaxChanged()
{
    if (spectrumXRangeMode() == 1) {
        emit spectrumManualXRangeChanged(spectrumManualXMin(), spectrumManualXMax());
    }
}

void ScaleControlDialog::onSpectrumManualYMinChanged()
{
    if (spectrumYRangeMode() == 1) {
        emit spectrumManualYRangeChanged(spectrumManualYMin(), spectrumManualYMax());
    }
}

void ScaleControlDialog::onSpectrumManualYMaxChanged()
{
    if (spectrumYRangeMode() == 1) {
        emit spectrumManualYRangeChanged(spectrumManualYMin(), spectrumManualYMax());
    }
}
