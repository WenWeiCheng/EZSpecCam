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

private slots:
    void onBrowseClicked();
    void onAutoSaveToggled(bool checked);
    void onImageFormatChanged(const QString &format);
    void onFrameSaveFormatChanged(const QString &format);
    void onSaveOriginalToggled(bool checked);

private:
    void setupUi();

    QLineEdit *autoSaveDirectoryLineEdit;
    QPushButton *browseDirectoryButton;
    QCheckBox *autoSaveEnabledCheckBox;
    QComboBox *imageFormatComboBox;
    QComboBox *frameSaveFormatComboBox;
    QCheckBox *saveOriginalDataCheckBox;
};

#endif // DATATAB_H