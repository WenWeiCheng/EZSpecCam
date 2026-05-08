#ifndef CAMERATAB_H
#define CAMERATAB_H

#include <QWidget>
#include <QComboBox>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QHash>
#include <QMap>
#include <QVBoxLayout>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QScrollArea>
#include <QTimer>
#include <QThread>
#include <QVariantMap>
#include <QPointer>

#include "../../AppController.h"
#include "CameraTypes.h"
#include "LoadingIndicator.h"

class CameraWorker;

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

    // Get current capture count based on mode selection
    // Returns: 0 for Live (continuous), 1 for Single, N for Burst
    int getCaptureCount() const;

    QPushButton *connectButton;
    QPushButton *disconnectButton;
    QComboBox *cameraComboBox;
    QComboBox *captureModeComboBox;
    QSpinBox *captureCountSpinBox;
    QFormLayout *formLayout;
    int m_countRow;
    QGroupBox *parameterGroup;

private slots:
    void onConnectButtonClicked();
    void onDisconnectButtonClicked();
    void onCameraSelected(int index);
    void onCameraStateChanged(CameraState state);
    void onCaptureModeChanged(int index);
    void onWorkerConnectionStateChanged(bool connected, const QString &cameraId, const QString &error);
    void onParametersCommitted();
    void onCoolingTimerTimeout();

private:
    void setupUi();
    void updateConnectionState();
    void clearDynamicParameterPanel();
    void applyCaptureMode();

    QPointer<AppController> m_appController;
    QThread *m_workerThread;
    CameraWorker *m_worker = nullptr;
    QLabel *m_statusLabel;
    QVariantMap m_bufferedConfig;

    QHash<QString, QWidget*> m_parameterWidgets;
    QMap<ParameterCategory, QGroupBox*> m_categoryGroups;
    QHash<QString, ParameterDefinition> m_parameterDefinitions;
    QVBoxLayout *m_dynamicParametersLayout;
    QScrollArea *m_scrollArea;
    QTimer *m_coolingTimer;
    LoadingIndicator *m_loadingIndicator = nullptr;
};

class CameraWorker : public QObject
{
    Q_OBJECT

public:
    explicit CameraWorker(AppController *controller, QObject *parent = nullptr);
    ~CameraWorker();

public slots:
    void doConnectCamera(const QString &cameraId);
    void doDisconnectCamera();
    void doSetParameters(const QVariantMap &params);

signals:
    void connectionStateChanged(bool connected, const QString &cameraId, const QString &error);
    void parametersCommitted(bool success, const QString &error);

private:
    AppController *m_controller;
};

#endif // CAMERATAB_H