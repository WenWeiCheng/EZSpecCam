#include "DataTab.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QSettings>

DataTab::DataTab(QWidget *parent)
    : QWidget(parent)
    , autoSaveDirectoryLineEdit(nullptr)
    , browseDirectoryButton(nullptr)
    , autoSaveEnabledCheckBox(nullptr)
    , imageFormatComboBox(nullptr)
    , frameSaveFormatComboBox(nullptr)
    , saveOriginalDataCheckBox(nullptr)
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
    imageFormatComboBox->addItems({"TIFF", "JPEG"});
    formLayout->addRow("Image Format:", imageFormatComboBox);

    frameSaveFormatComboBox = new QComboBox(this);
    frameSaveFormatComboBox->addItems({"Separate", "Embedded"});
    formLayout->addRow("Frame Save Format:", frameSaveFormatComboBox);

    saveOriginalDataCheckBox = new QCheckBox("Save original data (before post-processing)", this);
    formLayout->addRow("Save Original:", saveOriginalDataCheckBox);

    autoSaveGroup->setLayout(formLayout);
    mainLayout->addWidget(autoSaveGroup);

    mainLayout->addStretch();

    QSettings settings;
    autoSaveDirectoryLineEdit->setText(settings.value("data/autoSaveDirectory", "").toString());
    autoSaveEnabledCheckBox->setChecked(settings.value("data/autoSaveEnabled", false).toBool());
    imageFormatComboBox->setCurrentText(settings.value("data/imageFormat", "TIFF").toString());
    frameSaveFormatComboBox->setCurrentText(settings.value("data/frameSaveFormat", "Separate").toString());
    saveOriginalDataCheckBox->setChecked(settings.value("data/saveOriginalData", false).toBool());

    connect(browseDirectoryButton, &QPushButton::clicked,
            this, &DataTab::onBrowseClicked);
    connect(autoSaveEnabledCheckBox, &QCheckBox::toggled,
            this, &DataTab::onAutoSaveToggled);
    connect(imageFormatComboBox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &DataTab::onImageFormatChanged);
    connect(frameSaveFormatComboBox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &DataTab::onFrameSaveFormatChanged);
    connect(saveOriginalDataCheckBox, &QCheckBox::toggled,
            this, &DataTab::onSaveOriginalToggled);
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

void DataTab::onFrameSaveFormatChanged(const QString &format)
{
    Q_UNUSED(format);
    QSettings settings;
    settings.setValue("data/frameSaveFormat", format);
}

void DataTab::onSaveOriginalToggled(bool checked)
{
    QSettings settings;
    settings.setValue("data/saveOriginalData", checked);
}