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
    , m_okButton(nullptr)
    , m_cancelButton(nullptr)
{
    setWindowTitle("Calibration");
    setMinimumWidth(360);
    setModal(true);

    QFormLayout *formLayout = new QFormLayout(this);

    m_enableCheckBox = new QCheckBox(tr("Enable Dark-Frame Calibration"), this);
    formLayout->addRow(m_enableCheckBox);

    m_pathLineEdit = new QLineEdit(this);
    m_pathLineEdit->setReadOnly(true);
    m_pathLineEdit->setPlaceholderText(tr("(no dark frame selected)"));
    m_browseButton = new QPushButton(tr("Browse..."), this);

    QHBoxLayout *pathRow = new QHBoxLayout();
    pathRow->addWidget(m_pathLineEdit, 1);
    pathRow->addWidget(m_browseButton);
    formLayout->addRow(tr("Dark Frame:"), pathRow);

    m_biasSpinBox = new QSpinBox(this);
    m_biasSpinBox->setRange(-32768, 32767);
    m_biasSpinBox->setValue(0);
    m_biasSpinBox->setSingleStep(1);
    formLayout->addRow(tr("Custom Bias:"), m_biasSpinBox);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_okButton = new QPushButton(tr("OK"), this);
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
