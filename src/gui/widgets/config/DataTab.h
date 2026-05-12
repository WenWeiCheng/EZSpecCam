#ifndef DATATAB_H
#define DATATAB_H

#include <QWidget>

#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QComboBox>

class DataTab : public QWidget
{
    Q_OBJECT

public:
    explicit DataTab(QWidget *parent = nullptr);
    ~DataTab() override;

    QString prefix() const;
    QString suffix() const;
    bool saveMetadata() const;

private slots:
    void onBrowseClicked();
    void onAutoSaveToggled(bool checked);
    void onImageFormatChanged(const QString &format);
    void onSaveOriginalToggled(bool checked);
    void onSaveMetadataToggled(bool checked);
    void onPrefixChanged(const QString &prefix);
    void onSuffixChanged(const QString &suffix);

private:
    void setupUi();

    QLineEdit *autoSaveDirectoryLineEdit;
    QPushButton *browseDirectoryButton;
    QCheckBox *autoSaveEnabledCheckBox;
    QComboBox *imageFormatComboBox;
    QCheckBox *saveOriginalDataCheckBox;
    QCheckBox *saveMetadataCheckBox;
    QLineEdit *prefixLineEdit;
    QLineEdit *suffixLineEdit;
    QString m_prefix;
    QString m_suffix;
};

#endif // DATATAB_H