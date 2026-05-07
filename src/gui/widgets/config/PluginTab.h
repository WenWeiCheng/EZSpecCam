#ifndef PLUGINTAB_H
#define PLUGINTAB_H

#include <QWidget>

#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QAbstractItemView>

class AppController;
class CameraTab;

class PluginTab : public QWidget
{
    Q_OBJECT

public:
    explicit PluginTab(QWidget *parent = nullptr);
    ~PluginTab() override;

    void setCameraTab(CameraTab *cameraTab);
    void setAppController(AppController *controller);
    AppController *appController() const;

private slots:
    void onBrowseClicked();
    void onScanClicked();
    void onScanCompleted(int totalPlugins, int loadedPlugins);
    void onPluginLoadFailed(const QString &filePath, const QString &error);

private:
    void setupUi();
    void updatePluginsTable();

    AppController *m_appController;
    CameraTab *m_cameraTab;

    QLineEdit *pluginDirectoryLineEdit;
    QPushButton *browsePluginDirectoryButton;
    QPushButton *scanPluginsButton;
    QTableWidget *pluginsTableWidget;
};

#endif // PLUGINTAB_H