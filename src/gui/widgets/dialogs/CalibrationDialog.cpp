#include "CalibrationDialog.h"

#include <QFileDialog>
#include <QStandardPaths>

#include "../../../gui/workers/FileLoaderWorker.h"

CalibrationDialog::CalibrationDialog(QWidget *parent)
    : QDialog(parent)
    , m_enableCheckBox(nullptr)
    , m_pathLineEdit(nullptr)
    , m_browseButton(nullptr)
    , m_biasSpinBox(nullptr)
    , m_frameCountSpinBox(nullptr)
    , m_acquireButton(nullptr)
    , m_okButton(nullptr)
    , m_cancelButton(nullptr)
    , m_acquireInProgress(false)
    , m_acquireEnabled(true)
{
    setWindowTitle("Calibration");
    // 够宽才放得下「(in-memory dark frame, 10-frame average)」整条提示 ——
    // 采完暗帧之后用户第一眼看的就是这一行
    setMinimumWidth(540);
    // 采暗帧时主窗口还得能用（按 Stop、看实时画面），所以不能挡住主窗口
    setModal(false);

    QFormLayout *formLayout = new QFormLayout(this);

    m_enableCheckBox = new QCheckBox(tr("Enable Dark-Frame Calibration"), this);
    formLayout->addRow(m_enableCheckBox);

    m_pathLineEdit = new QLineEdit(this);
    m_pathLineEdit->setReadOnly(true);
    m_pathLineEdit->setPlaceholderText(tr("(no dark frame selected)"));
    m_browseButton = new QPushButton(tr("Browse..."), this);
    m_browseButton->setObjectName("browseButton");

    QHBoxLayout *pathRow = new QHBoxLayout();
    pathRow->addWidget(m_pathLineEdit, 1);
    pathRow->addWidget(m_browseButton);
    formLayout->addRow(tr("Dark Frame:"), pathRow);

    // 采集和上面那行是同一件事的两头：这里采完，上面的框当场变成
    // 「in-memory dark frame」，接着勾上面的 Enable 就能直接用，
    // 不用来回切窗口。
    m_frameCountSpinBox = new QSpinBox(this);
    m_frameCountSpinBox->setMinimum(1);
    m_frameCountSpinBox->setMaximum(10000);
    m_frameCountSpinBox->setValue(10);
    m_frameCountSpinBox->setSingleStep(1);
    m_frameCountSpinBox->setObjectName("frameCountSpinBox");
    m_acquireButton = new QPushButton(tr("Acquire"), this);
    m_acquireButton->setObjectName("acquireButton");

    // Acquire 和上一行的 Browse 一样靠右，两颗按钮才对得成一条竖线
    QHBoxLayout *acquireRow = new QHBoxLayout();
    acquireRow->addStretch(1);
    acquireRow->addWidget(m_frameCountSpinBox);
    acquireRow->addWidget(m_acquireButton);
    formLayout->addRow(tr("Frames to average:"), acquireRow);

    m_biasSpinBox = new QSpinBox(this);
    m_biasSpinBox->setRange(-32768, 32767);
    m_biasSpinBox->setValue(0);
    m_biasSpinBox->setSingleStep(1);
    formLayout->addRow(tr("Custom Bias:"), m_biasSpinBox);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_okButton = new QPushButton(tr("OK"), this);
    m_okButton->setObjectName("okButton");
    m_cancelButton = new QPushButton(tr("Cancel"), this);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    formLayout->addRow(buttonLayout);

    connect(m_browseButton, &QPushButton::clicked,
            this, &CalibrationDialog::onBrowseClicked);
    connect(m_okButton, &QPushButton::clicked,
            this, &CalibrationDialog::onOkClicked);
    connect(m_cancelButton, &QPushButton::clicked,
            this, &QDialog::reject);
    connect(m_acquireButton, &QPushButton::clicked,
            this, &CalibrationDialog::onAcquireClicked);
}

CalibrationDialog::~CalibrationDialog()
{
}

void CalibrationDialog::setDarkFrameEnabled(bool enabled)
{
    if (m_enableCheckBox) {
        m_enableCheckBox->setChecked(enabled);
    }
}

bool CalibrationDialog::darkFrameEnabled() const
{
    return m_enableCheckBox ? m_enableCheckBox->isChecked() : false;
}

void CalibrationDialog::setDarkFramePath(const QString &path)
{
    if (m_pathLineEdit) {
        m_pathLineEdit->setText(path);
    }
}

void CalibrationDialog::setInMemoryDarkFrameUsed(bool used, int frameCount)
{
    if (!m_pathLineEdit) {
        return;
    }
    m_pathLineEdit->setPlaceholderText(used
        ? tr("(in-memory dark frame, %1-frame average)").arg(frameCount)
        : tr("(no dark frame selected)"));
}

QString CalibrationDialog::darkFramePath() const
{
    return m_pathLineEdit ? m_pathLineEdit->text() : QString();
}

void CalibrationDialog::setCustomBias(int bias)
{
    if (m_biasSpinBox) {
        m_biasSpinBox->setValue(bias);
    }
}

int CalibrationDialog::customBias() const
{
    return m_biasSpinBox ? m_biasSpinBox->value() : 0;
}

int CalibrationDialog::frameCount() const
{
    return m_frameCountSpinBox ? m_frameCountSpinBox->value() : 10;
}

void CalibrationDialog::setFrameCount(int n)
{
    if (m_frameCountSpinBox) {
        m_frameCountSpinBox->setValue(n);
    }
}

void CalibrationDialog::setAcquireInProgress(bool busy)
{
    m_acquireInProgress = busy;
    if (m_frameCountSpinBox) {
        m_frameCountSpinBox->setEnabled(!busy);
    }
    if (m_acquireButton) {
        m_acquireButton->setEnabled(m_acquireEnabled && !busy);
    }
}

void CalibrationDialog::setAcquireEnabled(bool enabled)
{
    m_acquireEnabled = enabled;
    if (m_acquireButton) {
        m_acquireButton->setEnabled(enabled && !m_acquireInProgress);
    }
}

void CalibrationDialog::onBrowseClicked()
{
    QString openDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    QStringList filters{
        FileLoaderWorker::openFormatsDisplayName(),
        QStringLiteral("TIFF Image (*.tiff *.tif)"),
        QStringLiteral("CSV File (*.csv)"),
        QStringLiteral("All Files (*)")
    };

    QString path = QFileDialog::getOpenFileName(this,
        tr("Select Dark Frame"), openDir, filters.join(QStringLiteral(";;;")));

    if (!path.isEmpty()) {
        m_pathLineEdit->setText(path);
    }
}

void CalibrationDialog::onOkClicked()
{
    emit applied(darkFrameEnabled(), darkFramePath(), customBias());
    accept();
}

void CalibrationDialog::onAcquireClicked()
{
    if (m_acquireInProgress || !m_acquireEnabled) {
        return;
    }
    emit acquireRequested(frameCount());
    setAcquireInProgress(true);
}
