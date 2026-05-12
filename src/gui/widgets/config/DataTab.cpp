#include "DataTab.h"

#include <QFileDialog>
#include <QSettings>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>

DataTab::DataTab(QWidget *parent)
    : QWidget(parent)
    , ui(new DataTabUi(this))
    , m_prefix()
    , m_suffix()
{
    ui->setupUi(this);

    connect(ui->browseDirectoryButton, &QPushButton::clicked,
            this, &DataTab::onBrowseClicked);
    connect(ui->autoSaveEnabledCheckBox, &QCheckBox::toggled,
            this, &DataTab::onAutoSaveToggled);
    connect(ui->imageFormatComboBox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &DataTab::onImageFormatChanged);
    connect(ui->saveOriginalDataCheckBox, &QCheckBox::toggled,
            this, &DataTab::onSaveOriginalToggled);
    connect(ui->saveMetadataCheckBox, &QCheckBox::toggled,
            this, &DataTab::onSaveMetadataToggled);
    connect(ui->prefixLineEdit, &QLineEdit::textChanged,
            this, &DataTab::onPrefixChanged);
    connect(ui->suffixLineEdit, &QLineEdit::textChanged,
            this, &DataTab::onSuffixChanged);
}

DataTab::~DataTab()
{
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
    return ui->saveMetadataCheckBox->isChecked();
}

void DataTab::onBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Auto-Save Directory",
                                                  ui->autoSaveDirectoryLineEdit->text());
    if (!dir.isEmpty()) {
        ui->autoSaveDirectoryLineEdit->setText(dir);
        QSettings settings;
        settings.setValue(QStringLiteral("data/autoSaveDirectory"), dir);
    }
}

void DataTab::onAutoSaveToggled(bool checked)
{
    QSettings settings;
    settings.setValue(QStringLiteral("data/autoSaveEnabled"), checked);
}

void DataTab::onImageFormatChanged(const QString &format)
{
    Q_UNUSED(format);
    QSettings settings;
    settings.setValue(QStringLiteral("data/imageFormat"), format);
}

void DataTab::onSaveOriginalToggled(bool checked)
{
    QSettings settings;
    settings.setValue(QStringLiteral("data/saveOriginalData"), checked);
}

void DataTab::onSaveMetadataToggled(bool checked)
{
    QSettings settings;
    settings.setValue(QStringLiteral("data/saveMetadata"), checked);
}

void DataTab::onPrefixChanged(const QString &prefix)
{
    m_prefix = prefix;
    QSettings settings;
    settings.setValue(QStringLiteral("data/filenamePrefix"), prefix);
}

void DataTab::onSuffixChanged(const QString &suffix)
{
    m_suffix = suffix;
    QSettings settings;
    settings.setValue(QStringLiteral("data/filenameSuffix"), suffix);
}
