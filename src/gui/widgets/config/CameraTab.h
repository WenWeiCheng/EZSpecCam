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

class QLabel;
class QToolButton;
class ParameterHistoryStore;
class ParameterHistoryWindow;

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

    //! 历史曲线保留的采样点数（所有遥测参数共用），外部可随时修改
    int historyCapacity() const;
    void setHistoryCapacity(int points);
    ParameterHistoryStore *historyStore() const { return m_history; }

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
    void onTelemetryTimerTimeout();
    void onConnectCameraFinished(const QString &cameraId, bool success, const QString &error);
    void onDisconnectCameraFinished(const QString &cameraId);
    void onScanProgress(int current, int total, const QString &currentFile);
    void onScanStarted();
    void onScanCompleted(int totalPlugins, int loadedPlugins);
    void onSetParametersFinished(const QStringList &failedParameters);
    void onCommitParametersFinished(const QStringList &failedParameters);

private:
    //! createParameterField() 的产物。曲线按钮和值标签只在数值型遥测参数上存在
    struct ParameterField
    {
        QWidget *widget = nullptr;
        QLabel *label = nullptr;
        QToolButton *button = nullptr;
    };

    void updateConnectionState();
    void clearDynamicParameterPanel();
    void applyCaptureMode();
    ParameterField createParameterField(const QString &paramName, const ParameterDefinition &def);
    void registerParameterField(const QString &paramName, const ParameterField &field);
    void unregisterParameterField(const QString &paramName);
    void rebuildParameterWidget(const QString &paramName);
    void restoreWidgetsFromConfig(const QVariantMap &config, const QStringList &onlyNames = QStringList());
    void refreshCommittedConfigFromController();
    void updatePlotButtonIcons();
    void openHistoryWindow(const QString &paramName);

    QPointer<AppController> m_appController;
    QVariantMap m_bufferedConfig;
    QVariantMap m_committedConfig;

    QHash<QString, QWidget*> m_parameterWidgets;
    QMap<ParameterCategory, QGroupBox*> m_categoryGroups;
    QHash<QString, ParameterDefinition> m_parameterDefinitions;
    QTimer *m_telemetryTimer;
    int m_lastScanFailed = 0;

    QHash<QString, QLabel*> m_telemetryLabels;
    QHash<QString, QToolButton*> m_plotButtons;
    QHash<QString, ParameterHistoryWindow*> m_historyWindows;
    ParameterHistoryStore *m_history;
};

#endif // CAMERATAB_H
