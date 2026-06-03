#include "CustomRangeDialog.h"

#include <QLabel>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <algorithm>

CustomRangeDialog::CustomRangeDialog(QWidget *parent)
    : QDialog(parent)
    , m_imageWidth(0.0)
{
    setWindowTitle(tr("Custom X-Axis Range"));
    setMinimumWidth(350);

    auto *minLabel = new QLabel(tr("Minimum X value:"), this);
    m_minSpinBox = new QDoubleSpinBox(this);
    m_minSpinBox->setDecimals(2);
    m_minSpinBox->setRange(-1000000.0, 1000000.0);
    m_minSpinBox->setValue(0.0);

    auto *maxLabel = new QLabel(tr("Maximum X value:"), this);
    m_maxSpinBox = new QDoubleSpinBox(this);
    m_maxSpinBox->setDecimals(2);
    m_maxSpinBox->setRange(-1000000.0, 1000000.0);
    m_maxSpinBox->setValue(100.0);

    m_okButton = new QPushButton(tr("OK"), this);
    // Don't make OK the dialog's default button — pressing Enter inside
    // the spinbox should commit the value via editingFinished rather than
    // activating OK. The button is still activatable by mouse click or by
    // Tab-focusing it and pressing Enter/Space.
    m_okButton->setDefault(false);
    m_cancelButton = new QPushButton(tr("Cancel"), this);

    auto *minLayout = new QHBoxLayout();
    minLayout->addWidget(minLabel);
    minLayout->addWidget(m_minSpinBox);

    auto *maxLayout = new QHBoxLayout();
    maxLayout->addWidget(maxLabel);
    maxLayout->addWidget(m_maxSpinBox);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(minLayout);
    mainLayout->addLayout(maxLayout);
    mainLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    connect(m_okButton, &QPushButton::clicked, this, &CustomRangeDialog::onOkClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

CustomRangeDialog::~CustomRangeDialog()
{
}

void CustomRangeDialog::setRange(double min, double max)
{
    m_minSpinBox->setRange(min, max);
    m_maxSpinBox->setRange(min, max);
}

void CustomRangeDialog::setValues(double min, double max)
{
    m_minSpinBox->setValue(min);
    m_maxSpinBox->setValue(max);
}

void CustomRangeDialog::setImageWidth(double width)
{
    m_imageWidth = width;
    if (width > 0) {
        m_minSpinBox->setRange(0.0, width);
        m_maxSpinBox->setRange(0.0, width);
    }
}

double CustomRangeDialog::minValue() const
{
    return m_minSpinBox->value();
}

double CustomRangeDialog::maxValue() const
{
    return m_maxSpinBox->value();
}

void CustomRangeDialog::onOkClicked()
{
    double minVal = m_minSpinBox->value();
    double maxVal = m_maxSpinBox->value();

    if (m_imageWidth > 0) {
        if (minVal < 0) {
            minVal = 0;
            m_minSpinBox->setValue(minVal);
        }
        if (maxVal > m_imageWidth) {
            maxVal = m_imageWidth;
            m_maxSpinBox->setValue(maxVal);
        }
        if (minVal >= maxVal) {
            minVal = std::max(0.0, maxVal - 1);
            m_minSpinBox->setValue(minVal);
        }
    }

    if (minVal >= maxVal) {
        QMessageBox::warning(this, tr("Invalid Range"),
            tr("Minimum value must be less than maximum value."));
        return;
    }
    accept();
}