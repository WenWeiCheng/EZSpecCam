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

    // 行号按用户习惯用 1-based：第一行是 1，和 metadata 里的
    // vBinStartRow/vBinEndRow 以及 verticalBinning() 的解释一致，
    // 这样调用处不需要再做 ±1 转换
    m_startSpinBox = new QSpinBox(this);
    m_startSpinBox->setMinimum(1);
    m_startSpinBox->setValue(1);
    m_startSpinBox->setToolTip("Row number (1-based, first row is 1)");
    formLayout->addRow("Start Row (1-based):", m_startSpinBox);

    m_endSpinBox = new QSpinBox(this);
    m_endSpinBox->setMinimum(1);
    m_endSpinBox->setValue(1);
    m_endSpinBox->setToolTip("Row number (1-based, first row is 1)");
    formLayout->addRow("End Row (1-based):", m_endSpinBox);

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
        m_startSpinBox->setMinimum(1);
        m_startSpinBox->setMaximum(1);
        m_startSpinBox->setValue(1);
        m_startSpinBox->setEnabled(false);
        m_endSpinBox->setMinimum(1);
        m_endSpinBox->setMaximum(1);
        m_endSpinBox->setValue(1);
        m_endSpinBox->setEnabled(false);
        m_okButton->setEnabled(false);
        m_applyButton->setEnabled(false);
        return;
    }

    m_startSpinBox->setEnabled(true);
    m_endSpinBox->setEnabled(true);
    m_okButton->setEnabled(true);
    m_applyButton->setEnabled(true);

    // 1-based 闭区间：合法行号是 [1, height]
    m_startSpinBox->setMinimum(1);
    m_startSpinBox->setMaximum(height);
    m_endSpinBox->setMinimum(1);
    m_endSpinBox->setMaximum(height);
    m_endSpinBox->setValue(height);
}

void RowRangeDialog::setRange(int start, int end)
{
    if (m_imageHeight <= 0) {
        return;
    }
    m_startSpinBox->setValue(qBound(1, start, m_imageHeight));
    m_endSpinBox->setValue(qBound(1, end, m_imageHeight));
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
    if (applyRange()) {
        accept();
    }
}

void RowRangeDialog::onApplyClicked()
{
    applyRange();
}

bool RowRangeDialog::applyRange()
{
    const int start = m_startSpinBox->value();
    const int end = m_endSpinBox->value();
    if (start > end) {
        QMessageBox::warning(this, QStringLiteral("Invalid Range"),
                             QStringLiteral("Start row must be less than or equal to end row."));
        return false;
    }
    emit applyClicked(start, end);
    return true;
}

void RowRangeDialog::onHeightChanged(int height)
{
    // 这个槽接的是 Start Row 的 valueChanged，参数是新的起始行（不是图像高度）。
    // 起点超过终点时把终点顶到起点，避免 OK/Apply 直接被判非法
    if (height > 0 && m_endSpinBox->value() < m_startSpinBox->value()) {
        m_endSpinBox->setValue(height);
    }
}