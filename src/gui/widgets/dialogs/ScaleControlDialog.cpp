#include "ScaleControlDialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>

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
    m_imageScaleTypeCombo->addItem("image (linear)", 0);
    m_imageScaleTypeCombo->addItem("Log", 1);
    layout->addRow("Scale Type:", m_imageScaleTypeCombo);

    m_imageColorScaleModeCombo = new QComboBox(this);
    m_imageColorScaleModeCombo->addItem("Auto", 0);
    m_imageColorScaleModeCombo->addItem("8-bit (0-255)", 1);
    m_imageColorScaleModeCombo->addItem("16-bit (0-65535)", 2);
    layout->addRow("Color Scale:", m_imageColorScaleModeCombo);

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
    m_spectrumScaleTypeCombo->addItem("auto", 0);
    m_spectrumScaleTypeCombo->addItem("log", 1);
    layout->addRow("Scale Type:", m_spectrumScaleTypeCombo);

    connect(m_spectrumScaleTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScaleControlDialog::onSpectrumScaleTypeChanged);
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
