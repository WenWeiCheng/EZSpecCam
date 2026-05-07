#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QThread>
#include <QPluginLoader>
#include <QDir>
#include <QSharedPointer>
#include <QImage>

#include "ICameraDriver.h"
#include "CameraTypes.h"

/**
 * @class AppController
 * @brief Merged CameraManager + CameraPluginManager for EZSpecCam GUI
 *
 * This class combines the functionality of camera lifecycle management
 * (CameraManager) and plugin discovery/loading (CameraPluginManager)
 * into a single controller for the GUI layer.
 *
 * State Machine:
 *   Disconnected → Connecting → Connected → Acquiring → Error
 *        ↑_______________|          |___________|
 *
 * Threading:
 * - AppController lives on the main thread
 * - Driver signals are received via Qt::AutoConnection
 * - Frame processing happens on the main thread via queued slots
 */
class AppController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct the AppController
     * @param parent Parent QObject
     *
     * Initializes plugin scanning and sets up the state machine.
     */
    explicit AppController(QObject *parent = nullptr);

    /**
     * @brief Destructor
     *
     * Cleans up driver connection and unloads plugins.
     */
    ~AppController() override;

    // ——— Plugin Discovery (from CameraPluginManager) ———

    /**
     * @brief Scan for available camera driver plugins
     *
     * Discovers .dll files in the plugins/drivers directory and
     * loads valid ICameraDriver implementations.
     */
    void scanPlugins();

    /**
     * @brief Get list of all available cameras from loaded plugins
     * @return List of camera identifiers
     */
    QStringList availableCameras() const;

    /**
     * @brief Check if any plugins are loaded
     * @return true if plugins are available
     */
    bool hasPlugins() const;

    // ——— Connection Management ———

    /**
     * @brief Connect to a camera by ID
     * @param cameraId Camera identifier from availableCameras()
     * @return true if connection was initiated
     *
     * State transition: Disconnected/Error → Connecting → Connected
     */
    bool connectCamera(const QString &cameraId);

    /**
     * @brief Disconnect from the current camera
     *
     * State transition: Any → Disconnected
     */
    void disconnectCamera();

    /**
     * @brief Check if connected to a camera
     * @return true if connected and ready
     */
    bool isConnected() const;

    /**
     * @brief Check if the controller is in an error state
     * @return true if in error state
     */
    bool hasError() const;

    /**
     * @brief Get the currently connected camera ID
     * @return Camera ID string, or empty if not connected
     */
    QString currentCameraId() const;

    // ——— Capture Control ———

    /**
     * @brief Start capturing frames
     * @param captureCount Number of frames (0 = continuous)
     * @return true if capture started successfully
     *
     * State transition: Connected → Acquiring
     */
    bool startCapture(int captureCount = 0);

    /**
     * @brief Stop capturing frames
     * @param timeoutMs Maximum wait time in milliseconds
     *
     * State transition: Acquiring → Connected
     */
    void stopCapture(int timeoutMs = 5000);

    // ——— Parameter Management ———

    /**
     * @brief Get list of available parameter names
     * @return List of parameter identifiers
     */
    QStringList parameterNames() const;

    /**
     * @brief Get parameter definition
     * @param name Parameter identifier
     * @return ParameterDefinition struct
     */
    ParameterDefinition parameter(const QString &name) const;

    /**
     * @brief Get current parameter value
     * @param name Parameter identifier
     * @return Current value as QVariant
     */
    QVariant parameterValue(const QString &name) const;

    /**
     * @brief Set a parameter value
     * @param name Parameter identifier
     * @param value New value
     * @return true if value was accepted
     */
    bool setParameter(const QString &name, const QVariant &value);

    /**
     * @brief Validate all current parameters
     * @return true if all parameters are valid
     */
    bool validateParameters();

    /**
     * @brief Commit all staged parameter changes
     * @return true if commit succeeded
     */
    bool commitParameters();

    /**
     * @brief Save dynamic configuration to INI file
     * @param cameraId Camera identifier
     * @param parameters Map of parameters to save
     */
    void saveDynamicConfig(const QString &cameraId, const QVariantMap &parameters);

    /**
     * @brief Load dynamic configuration from INI file
     * @param cameraId Camera identifier
     * @return Map of loaded parameters
     */
    QVariantMap loadDynamicConfig(const QString &cameraId);

    /**
     * @brief Get all current parameters as a map
     * @return Map of parameter name → value
     */
    QVariantMap allParameters() const;

    /**
     * @brief Apply multiple parameters at once
     * @param params Map of parameters to apply
     * @return true if all parameters applied successfully
     */
    bool setParameters(const QVariantMap &params);

    // ——— State Access ———

    /**
     * @brief Get current camera state
     * @return CameraState enum value
     */
    CameraState state() const;

    /**
     * @brief Get current driver instance
     * @return Pointer to ICameraDriver, or nullptr
     */
    ICameraDriver *driver() const { return m_driver; }

signals:
    /**
     * @brief Emitted when camera connection state changes
     * @param connected true if connected, false if disconnected
     */
    void connectionChanged(bool connected);

    /**
     * @brief Emitted when camera state changes
     * @param newState New CameraState
     */
    void stateChanged(CameraState newState);

    /**
     * @brief Emitted when a new frame is available
     * @param frame ImageData containing the captured frame
     *
     * Note: This is emitted on the main thread via Qt::AutoConnection
     */
    void frameReady(const ImageData &frame);

    /**
     * @brief Emitted when an error occurs
     * @param error CameraError with error details
     */
    void errorOccurred(const CameraError &error);

    /**
     * @brief Emitted when capture starts
     */
    void captureStarted();

    /**
     * @brief Emitted when capture stops
     */
    void captureStopped();

    /**
     * @brief Emitted when plugin scan completes
     * @param totalPlugins Number of plugins found
     * @param loadedPlugins Number successfully loaded
     */
    void pluginScanCompleted(int totalPlugins, int loadedPlugins);

    /**
     * @brief Emitted when a plugin fails to load
     * @param filePath Plugin file path
     * @param error Error message
     */
    void pluginLoadFailed(const QString &filePath, const QString &error);

private slots:
    void onDriverFrameReady(const QSharedPointer<QImage> &image,
                           quint64 timestamp,
                           int frameNumber,
                           const QString &cameraId);
    void onDriverCaptureStarted(const QString &cameraId);
    void onDriverCaptureStopped(const QString &cameraId);
    void onDriverConnectionChanged(bool connected, const QString &cameraId);
    void onDriverError(const CameraError &error);

private:
    // State machine helpers
    void setState(CameraState newState);
    bool canConnect() const;
    bool canDisconnect() const;
    bool canStartCapture() const;
    bool canStopCapture() const;
    bool canConfigure() const;

    void enterConnectingState();
    void enterConnectedState();
    void enterAcquiringState();
    void enterDisconnectedState();
    void enterErrorState(const CameraError &error);

    void disconnectFromDriver();
    void cleanupDriver();

    // Plugin management
    struct PluginInfo {
        QString filePath;
        QStringList cameraIds;
        QPluginLoader *loader = nullptr;
        ICameraDriver *instance = nullptr;
    };

    const PluginInfo *findPluginForCamera(const QString &cameraId) const;
    bool loadPlugin(const QString &filePath);
    void unloadPlugin(const PluginInfo &info);
    void clearPlugins();

    // INI persistence helpers
    static QString getConfigDirectory();
    static QString getConfigPath(const QString &cameraId);
    bool saveParameters(const QString &filePath, const QString &cameraId, const QVariantMap &parameters);
    bool loadParameters(const QString &filePath, QString &cameraId, QVariantMap &parameters);
    QString variantToString(const QVariant &value);
    QVariant stringToVariant(const QString &valueStr);
    bool ensureDirectoryExists(const QString &dirPath);

    // State
    CameraState m_state = CameraState::Disconnected;
    QList<PluginInfo> m_plugins;
    QString m_pluginDir;
    ICameraDriver *m_driver = nullptr;
    QString m_cameraId;
    CameraError m_lastError;

    // Parameters cache
    QVariantMap m_parameters;

    // Capture tracking
    int m_captureCount = 0;
};

#endif // APPCONTROLLER_H