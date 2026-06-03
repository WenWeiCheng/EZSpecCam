#ifndef CAMERACONFIGDIALOG_H
#define CAMERACONFIGDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QShowEvent>
#include <QAbstractButton>
#include <QHash>
#include <QVariant>
#include <QMetaObject>

class CameraConfigDialogUi;
class AppController;

class CameraConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CameraConfigDialog(QWidget *parent = nullptr);
    ~CameraConfigDialog() override;

    void setAppController(AppController *controller);
    AppController *appController() const;
    CameraConfigDialogUi *getUi() const { return ui; }
    int getCaptureCount() const;

protected:
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

public slots:
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();
    void on_buttonBox_clicked(QAbstractButton *button);
    void on_restoreButton_clicked();
    void onSetParametersFinished(bool success);
    void onCommitParametersFinished(bool success);

private:
    CameraConfigDialogUi *ui;
    QVariantMap m_pendingConfig;
    bool m_acceptAfterCommit = false;
};

#endif // CAMERACONFIGDIALOG_H