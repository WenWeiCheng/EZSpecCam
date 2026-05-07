#ifndef CAMERACONFIGDIALOG_H
#define CAMERACONFIGDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QShowEvent>
#include <QAbstractButton>
#include <QThread>
#include <QHash>
#include <QVariant>

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

protected:
    void showEvent(QShowEvent *event) override;

public slots:
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();
    void on_buttonBox_clicked(QAbstractButton *button);
    void on_restoreButton_clicked();
    void onWorkerParametersCommitted(bool success, const QString &error);

private:
    CameraConfigDialogUi *ui;
    QThread *m_workerThread = nullptr;
    QHash<QString, QVariant> m_pendingConfig;
    bool m_acceptAfterCommit = false;
};

#endif // CAMERACONFIGDIALOG_H