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
    m_imageScaleTypeCombo->addItem("linear", 0);
    m_imageScaleTypeCombo->addItem("log", 1);
    layout->addRow("Type:", m_imageScaleTypeCombo);

    m_imageColorScaleModeCombo = new QComboBox(this);
    m_imageColorScaleModeCombo->addItem("Auto (min-max)", 0);
    m_imageColorScaleModeCombo->addItem("8-bit (0-255)", 1);
    m_imageColorScaleModeCombo->addItem("16-bit (0-65535)", 2);
    layout->addRow("Range:", m_imageColorScaleModeCombo);

    m_imageColorMapCombo = new QComboBox(this);
    m_imageColorMapCombo->addItem("Grayscale", 0);
    m_imageColorMapCombo->addItem("Hot", 1);
    m_imageColorMapCombo->addItem("Cold", 2);
    m_imageColorMapCombo->addItem("Night", 3);
    m_imageColorMapCombo->addItem("Candy", 4);
    m_imageColorMapCombo->addItem("Geography", 5);
    m_imageColorMapCombo->addItem("Ion", 6);
    m_imageColorMapCombo->addItem("Thermal", 7);
    m_imageColorMapCombo->addItem("Polar", 8);
    m_imageColorMapCombo->addItem("Spectrum", 9);
    m_imageColorMapCombo->addItem("Jet", 10);
    layout->addRow("Color Map:", m_imageColorMapCombo);

    connect(m_imageScaleTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScaleControlDialog::onImageScaleTypeChanged);
    connect(m_imageColorScaleModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScaleControlDialog::onImageColorScaleModeChanged);
    connect(m_imageColorMapCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScaleControlDialog::onImageColorMapChanged);
}

void ScaleControlDialog::createSpectrumGroup()
{
    m_spectrumGroup = new QGroupBox("Spectrum Scale", this);

    QFormLayout *layout = new QFormLayout(m_spectrumGroup);

    m_spectrumScaleTypeCombo = new QComboBox(this);
    m_spectrumScaleTypeCombo->addItem("linear", 0);
    m_spectrumScaleTypeCombo->addItem("log", 1);
    layout->addRow("Type:", m_spectrumScaleTypeCombo);

    m_spectrumLineStyleCombo = new QComboBox(this);
    m_spectrumLineStyleCombo->addItem("Line", 0);
    m_spectrumLineStyleCombo->addItem("Line + Points", 1);
    m_spectrumLineStyleCombo->addItem("Points", 2);
    layout->addRow("Style:", m_spectrumLineStyleCombo);

    connect(m_spectrumScaleTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScaleControlDialog::onSpectrumScaleTypeChanged);
    connect(m_spectrumLineStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScaleControlDialog::onSpectrumLineStyleChanged);
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

void ScaleControlDialog::setImageColorMap(int map)
{
    int index = m_imageColorMapCombo->findData(map);
    if (index >= 0) {
        m_imageColorMapCombo->blockSignals(true);
        m_imageColorMapCombo->setCurrentIndex(index);
        m_imageColorMapCombo->blockSignals(false);
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

void ScaleControlDialog::setSpectrumLineStyle(int style)
{
    int index = m_spectrumLineStyleCombo->findData(style);
    if (index >= 0) {
        m_spectrumLineStyleCombo->blockSignals(true);
        m_spectrumLineStyleCombo->setCurrentIndex(index);
        m_spectrumLineStyleCombo->blockSignals(false);
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

int ScaleControlDialog::imageColorMap() const
{
    return m_imageColorMapCombo->currentData().toInt();
}

int ScaleControlDialog::spectrumScaleType() const
{
    return m_spectrumScaleTypeCombo->currentData().toInt();
}

int ScaleControlDialog::spectrumLineStyle() const
{
    return m_spectrumLineStyleCombo->currentData().toInt();
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

void ScaleControlDialog::onImageColorMapChanged(int index)
{
    Q_UNUSED(index)
    emit imageColorMapChanged(imageColorMap());
}

void ScaleControlDialog::onSpectrumScaleTypeChanged(int index)
{
    Q_UNUSED(index)
    emit spectrumScaleTypeChanged(spectrumScaleType());
}

void ScaleControlDialog::onSpectrumLineStyleChanged(int index)
{
    Q_UNUSED(index)
    emit spectrumLineStyleChanged(spectrumLineStyle());
}
