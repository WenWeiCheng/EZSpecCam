#include "RowRangeDialog.h"

#include <QFormLayout>
#include <QMessageBox>

RowRangeDialog::RowRangeDialog(QWidget *parent)
    : QDialog(parent)
    , m_imageHeight(0)
{
    setWindowTitle("Select Row Range for Vertical Binning");
    setMinimumWidth(300);
    setModal(false);

    QFormLayout *formLayout = new QFormLayout(this);

    m_startSpinBox = new QSpinBox(this);
    m_startSpinBox->setMinimum(0);
    m_startSpinBox->setValue(0);
    formLayout->addRow("Start Row:", m_startSpinBox);

    m_endSpinBox = new QSpinBox(this);
    m_endSpinBox->setMinimum(0);
    m_endSpinBox->setValue(0);
    formLayout->addRow("End Row:", m_endSpinBox);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_okButton = new QPushButton("OK", this);
    m_applyButton = new QPushButton("Apply", this);
    m_cancelButton = new QPushButton("Cancel", this);
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_applyButton);
    buttonLayout->addWidget(m_cancelButton);

    formLayout->addRow(buttonLayout);

    connect(m_okButton, &QPushButton::clicked, this, &RowRangeDialog::onOkClicked);
    connect(m_applyButton, &QPushButton::clicked, this, &RowRangeDialog::onApplyClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_startSpinBox, &QSpinBox::valueChanged, this, &RowRangeDialog::onHeightChanged);
}

RowRangeDialog::~RowRangeDialog()
{
}

void RowRangeDialog::setImageHeight(int height)
{
    m_imageHeight = height;
    int maxRow = qMax(0, height - 1);

    m_startSpinBox->setMaximum(maxRow);
    m_endSpinBox->setMaximum(maxRow);

    if (height > 0 && m_endSpinBox->value() == 0 && m_startSpinBox->value() == 0) {
        m_endSpinBox->setValue(maxRow);
    }
}

void RowRangeDialog::setRange(int start, int end)
{
    m_startSpinBox->setValue(start);
    m_endSpinBox->setValue(end);
}

int RowRangeDialog::startRow() const
{
    return m_startSpinBox->value();
}

int RowRangeDialog::endRow() const
{
    return m_endSpinBox->value();
}

void RowRangeDialog::onOkClicked()
{
    if (m_startSpinBox->value() > m_endSpinBox->value()) {
        QMessageBox::warning(this, "Invalid Range", "Start row must be less than or equal to end row.");
        return;
    }
    accept();
}

void RowRangeDialog::onApplyClicked()
{
    if (m_startSpinBox->value() > m_endSpinBox->value()) {
        QMessageBox::warning(this, "Invalid Range", "Start row must be less than or equal to end row.");
        return;
    }
    emit applyClicked(m_startSpinBox->value(), m_endSpinBox->value());
}

void RowRangeDialog::onHeightChanged(int height)
{
    if (height > 0 && m_endSpinBox->value() < m_startSpinBox->value()) {
        m_endSpinBox->setValue(height - 1);
    }
}