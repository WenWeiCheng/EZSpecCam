#ifndef CAMERATAB_H
#define CAMERATAB_H

#include <QWidget>
#include <QHash>
#include <QMap>
#include <QTimer>
#include <QVariantMap>
#include <QPointer>
#include <QSettings>
#include <QMetaObject>
#include <QGroupBox>

#include "../../AppController.h"
#include "CameraTypes.h"
#include "LoadingIndicator.h"
#include "../../ui/CameraTabUi.h"

class CameraTab : public QWidget
{
    Q_OBJECT

public:
    explicit CameraTab(QWidget *parent = nullptr);
    ~CameraTab() override;

    void setAppController(AppController *controller);
    AppController *appController() const { return m_appController; }

    void refreshCameraList();

    void setBufferedConfig(const QVariantMap &config);
    QVariantMap getBufferedConfig() const;
    void updateBufferedConfigFromWidgets();
    void buildDynamicParameterPanel();

    int getCaptureCount() const;

    // 「Capture Mode」下拉框是采集模式的唯一出处：L / S / B 快捷键改的就是它，
    // MainWindow 状态栏上显示的 Mode 也是从它来的
    QString getCaptureMode() const;
    void setCaptureMode(const QString &mode);

    CameraTabUi *ui = nullptr;

signals:
    void captureModeChanged(const QString &mode);

protected slots:
    void onScanButtonClicked();
    void onConnectButtonClicked();
    void onDisconnectButtonClicked();
    void onCameraSelected(int index);
    void onCameraStateChanged(CameraState state);
    void onCaptureModeChanged(int index);
    void onParametersCommitted();
    void onCoolingTimerTimeout();
    void onConnectCameraFinished(const QString &cameraId, bool success, const QString &error);
    void onDisconnectCameraFinished(const QString &cameraId);
    void onScanProgress(int current, int total, const QString &currentFile);
    void onScanStarted();
    void onScanCompleted(int totalPlugins, int loadedPlugins);
    void onSetParametersFinished(const QStringList &failedParameters);
    void onCommitParametersFinished(const QStringList &failedParameters);

private:
    void updateConnectionState();
    void clearDynamicParameterPanel();
    void applyCaptureMode();
    void rebuildParameterWidget(const QString &paramName);
    void restoreWidgetsFromConfig(const QVariantMap &config, const QStringList &onlyNames = QStringList());
    void refreshCommittedConfigFromController();

    QPointer<AppController> m_appController;
    QVariantMap m_bufferedConfig;
    QVariantMap m_committedConfig;

    QHash<QString, QWidget*> m_parameterWidgets;
    QMap<ParameterCategory, QGroupBox*> m_categoryGroups;
    QHash<QString, ParameterDefinition> m_parameterDefinitions;
    QTimer *m_coolingTimer;
    int m_lastScanFailed = 0;
};

#endif // CAMERATAB_H