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

QString DataTab::autoSaveDirectory() const
{
    return ui->autoSaveDirectoryLineEdit ? ui->autoSaveDirectoryLineEdit->text() : QString();
}

void DataTab::setAutoSaveDirectory(const QString &dir)
{
    if (!ui->autoSaveDirectoryLineEdit || ui->autoSaveDirectoryLineEdit->text() == dir) {
        return;
    }
    const QSignalBlocker blocker(ui->autoSaveDirectoryLineEdit);
    ui->autoSaveDirectoryLineEdit->setText(dir);
}

bool DataTab::isAutoSaveEnabled() const
{
    return ui->autoSaveEnabledCheckBox && ui->autoSaveEnabledCheckBox->isChecked();
}

void DataTab::setAutoSaveEnabled(bool enabled)
{
    if (!ui->autoSaveEnabledCheckBox || ui->autoSaveEnabledCheckBox->isChecked() == enabled) {
        return;
    }
    const QSignalBlocker blocker(ui->autoSaveEnabledCheckBox);
    ui->autoSaveEnabledCheckBox->setChecked(enabled);
}

void DataTab::onBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Auto-Save Directory",
                                                  ui->autoSaveDirectoryLineEdit->text());
    if (!dir.isEmpty()) {
        ui->autoSaveDirectoryLineEdit->setText(dir);
        QSettings settings;
        settings.setValue(QStringLiteral("data/autoSaveDirectory"), dir);
        emit autoSaveDirectoryChanged(dir);
    }
}

void DataTab::onAutoSaveToggled(bool checked)
{
    QSettings settings;
    settings.setValue(QStringLiteral("data/autoSaveEnabled"), checked);
    emit autoSaveToggled(checked);
}

void DataTab::onImageFormatChanged(const QString &format)
{
    QSettings settings;
    settings.setValue(QStringLiteral("data/imageFormat"), format);
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
