#include "DisplayStyleDialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

DisplayStyleDialog::DisplayStyleDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Display Style");
    setModal(false);
    setMinimumWidth(300);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    createImageGroup();
    createSpectrumGroup();

    mainLayout->addWidget(m_imageGroup);
    mainLayout->addWidget(m_spectrumGroup);
    mainLayout->addStretch();
}

DisplayStyleDialog::~DisplayStyleDialog()
{
}

void DisplayStyleDialog::createImageGroup()
{
    m_imageGroup = new QGroupBox("Image Display", this);

    QFormLayout *layout = new QFormLayout(m_imageGroup);

    m_showColorScaleCheck = new QCheckBox("Show Color Scale", this);
    m_showColorScaleCheck->setChecked(true);
    layout->addRow(m_showColorScaleCheck);

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

    connect(m_showColorScaleCheck, &QCheckBox::toggled,
            this, &DisplayStyleDialog::onColorScaleToggled);
    connect(m_imageColorMapCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DisplayStyleDialog::onImageColorMapChanged);
}

void DisplayStyleDialog::createSpectrumGroup()
{
    m_spectrumGroup = new QGroupBox("Spectrum Display", this);

    QFormLayout *layout = new QFormLayout(m_spectrumGroup);

    m_spectrumLineStyleCombo = new QComboBox(this);
    m_spectrumLineStyleCombo->addItem("Line", 0);
    m_spectrumLineStyleCombo->addItem("Line + Points", 1);
    m_spectrumLineStyleCombo->addItem("Points", 2);
    layout->addRow("Style:", m_spectrumLineStyleCombo);

    connect(m_spectrumLineStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DisplayStyleDialog::onSpectrumLineStyleChanged);
}

void DisplayStyleDialog::setColorScaleVisible(bool visible)
{
    m_showColorScaleCheck->blockSignals(true);
    m_showColorScaleCheck->setChecked(visible);
    m_showColorScaleCheck->blockSignals(false);
}

void DisplayStyleDialog::setImageColorMap(int map)
{
    int index = m_imageColorMapCombo->findData(map);
    if (index >= 0) {
        m_imageColorMapCombo->blockSignals(true);
        m_imageColorMapCombo->setCurrentIndex(index);
        m_imageColorMapCombo->blockSignals(false);
    }
}

void DisplayStyleDialog::setSpectrumLineStyle(int style)
{
    int index = m_spectrumLineStyleCombo->findData(style);
    if (index >= 0) {
        m_spectrumLineStyleCombo->blockSignals(true);
        m_spectrumLineStyleCombo->setCurrentIndex(index);
        m_spectrumLineStyleCombo->blockSignals(false);
    }
}

bool DisplayStyleDialog::isColorScaleVisible() const
{
    return m_showColorScaleCheck->isChecked();
}

int DisplayStyleDialog::imageColorMap() const
{
    return m_imageColorMapCombo->currentData().toInt();
}

int DisplayStyleDialog::spectrumLineStyle() const
{
    return m_spectrumLineStyleCombo->currentData().toInt();
}

void DisplayStyleDialog::onColorScaleToggled(bool checked)
{
    emit colorScaleToggled(checked);
}

void DisplayStyleDialog::onImageColorMapChanged(int index)
{
    Q_UNUSED(index)
    emit imageColorMapChanged(imageColorMap());
}

void DisplayStyleDialog::onSpectrumLineStyleChanged(int index)
{
    Q_UNUSED(index)
    emit spectrumLineStyleChanged(spectrumLineStyle());
}
