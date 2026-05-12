#include "DataTab.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QSettings>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

DataTab::DataTab(QWidget *parent)
    : QWidget(parent)
    , autoSaveDirectoryLineEdit(nullptr)
    , browseDirectoryButton(nullptr)
    , autoSaveEnabledCheckBox(nullptr)
    , imageFormatComboBox(nullptr)
    , saveOriginalDataCheckBox(nullptr)
    , saveMetadataCheckBox(nullptr)
    , prefixLineEdit(nullptr)
    , suffixLineEdit(nullptr)
    , m_prefix()
    , m_suffix()
{
    setupUi();
}

DataTab::~DataTab()
{
}

// TODO: UI 放在专门的文件里组织
void DataTab::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QGroupBox *autoSaveGroup = new QGroupBox("Auto-Save Settings", this);

    QFormLayout *formLayout = new QFormLayout();

    autoSaveEnabledCheckBox = new QCheckBox("Enable auto-save", this);
    formLayout->addRow("Auto-Save:", autoSaveEnabledCheckBox);

    QHBoxLayout *directoryLayout = new QHBoxLayout();
    autoSaveDirectoryLineEdit = new QLineEdit(this);
    autoSaveDirectoryLineEdit->setPlaceholderText("Select directory...");
    browseDirectoryButton = new QPushButton("Browse...", this);
    directoryLayout->addWidget(autoSaveDirectoryLineEdit);
    directoryLayout->addWidget(browseDirectoryButton);
    formLayout->addRow("Directory:", directoryLayout);

    imageFormatComboBox = new QComboBox(this);
    imageFormatComboBox->addItems({"TIFF", "CSV"});
    formLayout->addRow("Image Format:", imageFormatComboBox);

    saveOriginalDataCheckBox = new QCheckBox("Save original data (before post-processing)", this);
    formLayout->addRow("Save Original:", saveOriginalDataCheckBox);

    saveMetadataCheckBox = new QCheckBox("Save metadata as JSON file alongside image", this);
    formLayout->addRow("Save Metadata:", saveMetadataCheckBox);

    prefixLineEdit = new QLineEdit(this);
    QRegularExpression regex("^[a-zA-Z0-9_\u4e00-\u9fa5]*$");
    prefixLineEdit->setValidator(new QRegularExpressionValidator(regex, this));
    formLayout->addRow("Prefix:", prefixLineEdit);

    suffixLineEdit = new QLineEdit(this);
    suffixLineEdit->setValidator(new QRegularExpressionValidator(regex, this));
    formLayout->addRow("Suffix:", suffixLineEdit);

    autoSaveGroup->setLayout(formLayout);
    mainLayout->addWidget(autoSaveGroup);

    mainLayout->addStretch();

    QSettings settings;
    autoSaveDirectoryLineEdit->setText(settings.value("data/autoSaveDirectory", "").toString());
    autoSaveEnabledCheckBox->setChecked(settings.value("data/autoSaveEnabled", false).toBool());
    imageFormatComboBox->setCurrentText(settings.value("data/imageFormat", "TIFF").toString());
    saveOriginalDataCheckBox->setChecked(settings.value("data/saveOriginalData", false).toBool());
    saveMetadataCheckBox->setChecked(settings.value("data/saveMetadata", true).toBool());
    m_prefix = settings.value("data/filenamePrefix", "").toString();
    m_suffix = settings.value("data/filenameSuffix", "").toString();
    prefixLineEdit->setText(m_prefix);
    suffixLineEdit->setText(m_suffix);

    connect(browseDirectoryButton, &QPushButton::clicked,
            this, &DataTab::onBrowseClicked);
    connect(autoSaveEnabledCheckBox, &QCheckBox::toggled,
            this, &DataTab::onAutoSaveToggled);
    connect(imageFormatComboBox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &DataTab::onImageFormatChanged);
    connect(saveOriginalDataCheckBox, &QCheckBox::toggled,
            this, &DataTab::onSaveOriginalToggled);
    connect(saveMetadataCheckBox, &QCheckBox::toggled,
            this, &DataTab::onSaveMetadataToggled);
    connect(prefixLineEdit, &QLineEdit::textChanged,
            this, &DataTab::onPrefixChanged);
    connect(suffixLineEdit, &QLineEdit::textChanged,
            this, &DataTab::onSuffixChanged);
}

void DataTab::onBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Auto-Save Directory",
                                                  autoSaveDirectoryLineEdit->text());
    if (!dir.isEmpty()) {
        autoSaveDirectoryLineEdit->setText(dir);
        QSettings settings;
        settings.setValue("data/autoSaveDirectory", dir);
    }
}

void DataTab::onAutoSaveToggled(bool checked)
{
    QSettings settings;
    settings.setValue("data/autoSaveEnabled", checked);
}

void DataTab::onImageFormatChanged(const QString &format)
{
    Q_UNUSED(format);
    QSettings settings;
    settings.setValue("data/imageFormat", format);
}

void DataTab::onSaveOriginalToggled(bool checked)
{
    QSettings settings;
    settings.setValue("data/saveOriginalData", checked);
}

void DataTab::onSaveMetadataToggled(bool checked)
{
    QSettings settings;
    settings.setValue("data/saveMetadata", checked);
}

void DataTab::onPrefixChanged(const QString &prefix)
{
    m_prefix = prefix;
    QSettings settings;
    settings.setValue("data/filenamePrefix", prefix);
}

void DataTab::onSuffixChanged(const QString &suffix)
{
    m_suffix = suffix;
    QSettings settings;
    settings.setValue("data/filenameSuffix", suffix);
}

QString DataTab::prefix() const
{
    return m_prefix;
}

QString DataTab::suffix() const
{
    return m_suffix;
}

bool DataTab::saveMetadata() const
{
    QSettings settings;
    return settings.value("data/saveMetadata", true).toBool();
}