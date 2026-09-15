#include "AcquireDarkFrameDialog.h"

AcquireDarkFrameDialog::AcquireDarkFrameDialog(QWidget *parent)
    : QDialog(parent)
    , m_frameCountSpinBox(nullptr)
    , m_startButton(nullptr)
    , m_closeButton(nullptr)
    , m_acquireInProgress(false)
{
    setWindowTitle("Acquire Dark Frame");
    setMinimumWidth(300);
    setModal(false);

    QFormLayout *formLayout = new QFormLayout(this);

    m_frameCountSpinBox = new QSpinBox(this);
    m_frameCountSpinBox->setMinimum(1);
    m_frameCountSpinBox->setMaximum(1000);
    m_frameCountSpinBox->setValue(10);
    m_frameCountSpinBox->setSingleStep(1);
    formLayout->addRow(tr("Number of frames to average:"), m_frameCountSpinBox);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_startButton = new QPushButton(tr("Start"), this);
    m_closeButton = new QPushButton(tr("Close"), this);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_closeButton);
    formLayout->addRow(buttonLayout);

    connect(m_startButton, &QPushButton::clicked,
            this, &AcquireDarkFrameDialog::onStartClicked);
    connect(m_closeButton, &QPushButton::clicked,
            this, &QDialog::reject);
}

AcquireDarkFrameDialog::~AcquireDarkFrameDialog()
{
}

int AcquireDarkFrameDialog::frameCount() const
{
    return m_frameCountSpinBox ? m_frameCountSpinBox->value() : 10;
}

void AcquireDarkFrameDialog::setFrameCount(int n)
{
    if (m_frameCountSpinBox) {
        m_frameCountSpinBox->setValue(n);
    }
}

void AcquireDarkFrameDialog::setAcquireInProgress(bool busy)
{
    m_acquireInProgress = busy;
    if (m_startButton) {
        m_startButton->setEnabled(!busy);
    }
}

void AcquireDarkFrameDialog::onStartClicked()
{
    if (m_acquireInProgress) {
        return;
    }
    const int n = frameCount();
    emit startRequested(n);
    setAcquireInProgress(true);
}
