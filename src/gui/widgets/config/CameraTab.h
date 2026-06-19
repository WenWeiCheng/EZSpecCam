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

    CameraTabUi *ui = nullptr;

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
    void onScanCompleted(int totalPlugins, int loadedPlugins);

private:
    void updateConnectionState();
    void clearDynamicParameterPanel();
    void applyCaptureMode();
    void rebuildParameterWidget(const QString &paramName);

    QPointer<AppController> m_appController;
    QVariantMap m_bufferedConfig;

    QHash<QString, QWidget*> m_parameterWidgets;
    QMap<ParameterCategory, QGroupBox*> m_categoryGroups;
    QHash<QString, ParameterDefinition> m_parameterDefinitions;
    QTimer *m_coolingTimer;
    int m_lastScanFailed = 0;
};

#endif // CAMERATAB_H