#ifndef CALIBRATIONDIALOG_H
#define CALIBRATIONDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QFormLayout>
#include <QHBoxLayout>

class CalibrationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CalibrationDialog(QWidget *parent = nullptr);
    ~CalibrationDialog() override;

    void setDarkFrameEnabled(bool enabled);
    bool darkFrameEnabled() const;

    void setDarkFramePath(const QString &path);
    QString darkFramePath() const;

    void setCustomBias(int bias);
    int customBias() const;

signals:
    void applied(bool enabled, const QString &filePath, int customBias);

private slots:
    void onBrowseClicked();
    void onOkClicked();

private:
    QCheckBox *m_enableCheckBox;
    QLineEdit *m_pathLineEdit;
    QPushButton *m_browseButton;
    QSpinBox *m_biasSpinBox;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
};

#endif // CALIBRATIONDIALOG_H
