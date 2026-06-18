#ifndef DATATAB_H
#define DATATAB_H

#include <QWidget>
#include "../../ui/DataTabUi.h"

class DataTab : public QWidget
{
    Q_OBJECT

public:
    explicit DataTab(QWidget *parent = nullptr);
    ~DataTab() override;

    QString prefix() const;
    QString suffix() const;

protected slots:
    void onBrowseClicked();
    void onAutoSaveToggled(bool checked);
    void onImageFormatChanged(const QString &format);
    void onPrefixChanged(const QString &prefix);
    void onSuffixChanged(const QString &suffix);

private:
    DataTabUi *ui;
    QString m_prefix;
    QString m_suffix;
};

#endif
