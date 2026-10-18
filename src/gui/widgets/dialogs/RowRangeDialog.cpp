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

    // 行号用 0-based，和图像像素索引一致：Profile 窗口标题里的 "Row Y=439"
    // 就是这个值，直接填进来就能得到同一行的谱线
    m_startSpinBox = new QSpinBox(this);
    m_startSpinBox->setMinimum(0);
    m_startSpinBox->setValue(0);
    m_startSpinBox->setToolTip("Row index as shown in the Profile window (0-based)");
    formLayout->addRow("Start Row (0-based):", m_startSpinBox);

    m_endSpinBox = new QSpinBox(this);
    m_endSpinBox->setMinimum(0);
    m_endSpinBox->setValue(0);
    m_endSpinBox->setToolTip("Row index as shown in the Profile window (0-based)");
    formLayout->addRow("End Row (0-based):", m_endSpinBox);

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

    if (height <= 0) {
        m_startSpinBox->setMinimum(0);
        m_startSpinBox->setMaximum(0);
        m_startSpinBox->setValue(0);
        m_startSpinBox->setEnabled(false);
        m_endSpinBox->setMinimum(0);
        m_endSpinBox->setMaximum(0);
        m_endSpinBox->setValue(0);
        m_endSpinBox->setEnabled(false);
        m_okButton->setEnabled(false);
        m_applyButton->setEnabled(false);
        return;
    }

    m_startSpinBox->setEnabled(true);
    m_endSpinBox->setEnabled(true);
    m_okButton->setEnabled(true);
    m_applyButton->setEnabled(true);

    // 0-based 闭区间：合法行号是 [0, height - 1]
    m_startSpinBox->setMinimum(0);
    m_startSpinBox->setMaximum(height - 1);
    m_endSpinBox->setMinimum(0);
    m_endSpinBox->setMaximum(height - 1);
    m_endSpinBox->setValue(height - 1);
}

void RowRangeDialog::setRange(int start, int end)
{
    if (m_imageHeight <= 0) {
        return;
    }
    m_startSpinBox->setValue(qBound(0, start, m_imageHeight - 1));
    m_endSpinBox->setValue(qBound(0, end, m_imageHeight - 1));
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
    emit applyClicked(m_startSpinBox->value(), m_endSpinBox->value());
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
    // 这个槽接的是 Start Row 的 valueChanged，参数是新的起始行（不是图像高度）。
    // 起点超过终点时把终点顶到起点，避免 OK/Apply 直接被判非法
    if (height > 0 && m_endSpinBox->value() < m_startSpinBox->value()) {
        m_endSpinBox->setValue(height);
    }
}