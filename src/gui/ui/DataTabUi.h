#ifndef DATATABUI_H
#define DATATABUI_H

#include <QObject>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QComboBox>

class DataTab;

class DataTabUi : public QObject
{
    Q_OBJECT
public:
    explicit DataTabUi(QObject *parent = nullptr);
    ~DataTabUi();

    void setupUi(DataTab *tab);

    QLineEdit *autoSaveDirectoryLineEdit;
    QPushButton *browseDirectoryButton;
    QCheckBox *autoSaveEnabledCheckBox;
    QComboBox *imageFormatComboBox;
    QLineEdit *prefixLineEdit;
    QLineEdit *suffixLineEdit;

private:
    QObject *m_parent;
};

#endif
