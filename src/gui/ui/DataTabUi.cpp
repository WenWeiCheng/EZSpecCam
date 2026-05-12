#include "DataTabUi.h"
#include "../widgets/config/DataTab.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSettings>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

DataTabUi::DataTabUi(QObject *parent)
    : QObject(parent)
    , autoSaveDirectoryLineEdit(nullptr)
    , browseDirectoryButton(nullptr)
    , autoSaveEnabledCheckBox(nullptr)
    , imageFormatComboBox(nullptr)
    , saveOriginalDataCheckBox(nullptr)
    , saveMetadataCheckBox(nullptr)
    , prefixLineEdit(nullptr)
    , suffixLineEdit(nullptr)
    , m_parent(parent)
{
}

DataTabUi::~DataTabUi()
{
}

void DataTabUi::setupUi(DataTab *tab)
{
    if (!tab) {
        return;
    }

    QVBoxLayout *mainLayout = new QVBoxLayout(tab);

    QGroupBox *autoSaveGroup = new QGroupBox("Auto-Save Settings", tab);

    QFormLayout *formLayout = new QFormLayout();

    autoSaveEnabledCheckBox = new QCheckBox("Enable auto-save", tab);
    formLayout->addRow("Auto-Save:", autoSaveEnabledCheckBox);

    QHBoxLayout *directoryLayout = new QHBoxLayout();
    autoSaveDirectoryLineEdit = new QLineEdit(tab);
    autoSaveDirectoryLineEdit->setPlaceholderText("Select directory...");
    browseDirectoryButton = new QPushButton("Browse...", tab);
    directoryLayout->addWidget(autoSaveDirectoryLineEdit);
    directoryLayout->addWidget(browseDirectoryButton);
    formLayout->addRow("Directory:", directoryLayout);

    imageFormatComboBox = new QComboBox(tab);
    imageFormatComboBox->addItems({"TIFF", "CSV"});
    formLayout->addRow("Image Format:", imageFormatComboBox);

    saveOriginalDataCheckBox = new QCheckBox("Save original data (before post-processing)", tab);
    formLayout->addRow("Save Original:", saveOriginalDataCheckBox);

    saveMetadataCheckBox = new QCheckBox("Save metadata as JSON file alongside image", tab);
    formLayout->addRow("Save Metadata:", saveMetadataCheckBox);

    prefixLineEdit = new QLineEdit(tab);
    QRegularExpression regex("^[a-zA-Z0-9_\u4e00-\u9fa5]*$");
    prefixLineEdit->setValidator(new QRegularExpressionValidator(regex, tab));
    formLayout->addRow("Prefix:", prefixLineEdit);

    suffixLineEdit = new QLineEdit(tab);
    suffixLineEdit->setValidator(new QRegularExpressionValidator(regex, tab));
    formLayout->addRow("Suffix:", suffixLineEdit);

    autoSaveGroup->setLayout(formLayout);
    mainLayout->addWidget(autoSaveGroup);

    mainLayout->addStretch();

    QSettings settings;
    autoSaveDirectoryLineEdit->setText(settings.value(QStringLiteral("data/autoSaveDirectory"), QString()).toString());
    autoSaveEnabledCheckBox->setChecked(settings.value(QStringLiteral("data/autoSaveEnabled"), false).toBool());
    imageFormatComboBox->setCurrentText(settings.value(QStringLiteral("data/imageFormat"), QStringLiteral("TIFF")).toString());
    saveOriginalDataCheckBox->setChecked(settings.value(QStringLiteral("data/saveOriginalData"), false).toBool());
    saveMetadataCheckBox->setChecked(settings.value(QStringLiteral("data/saveMetadata"), true).toBool());
    prefixLineEdit->setText(settings.value(QStringLiteral("data/filenamePrefix"), QString()).toString());
    suffixLineEdit->setText(settings.value(QStringLiteral("data/filenameSuffix"), QString()).toString());
}
