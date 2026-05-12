#ifndef CAMERACONFIGDIALOGUI_H
#define CAMERACONFIGDIALOGUI_H

#include <QDialogButtonBox>
#include <QDialog>
#include <QTabWidget>
#include <QObject>
#include <QLabel>

class CameraTab;
class DataTab;
class PluginTab;
class AppController;
class LoadingIndicator;

class CameraConfigDialogUi : public QObject
{
    Q_OBJECT
public:
    explicit CameraConfigDialogUi(QObject *parent = nullptr);
    ~CameraConfigDialogUi();

    void setupUi(QDialog *dialog);
    void setAppController(AppController *controller);

    void showLoading(const QString &statusText);
    void hideLoading();

    QTabWidget *tabWidget;
    QDialogButtonBox *buttonBox;
    LoadingIndicator *loadingIndicator;
    QLabel *loadingStatusLabel;

    CameraTab *cameraTab;
    DataTab *dataTab;
    PluginTab *pluginTab;

private:
    void createTabs(QDialog *dialog);
    void createButtonBox(QDialog *dialog);

    QObject *m_parent;
};

#endif // CAMERACONFIGDIALOGUI_H
