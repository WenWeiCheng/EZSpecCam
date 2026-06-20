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

// Plugin management
struct PluginInfo {
    QString filePath;
    QStringList cameraIds;
    QPluginLoader *loader = nullptr;
    ICameraDriver *instance = nullptr;
};

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
 * - AppController lives on a dedicated QThread (managed by MainWindow)
 * - Driver signals are received via Qt::DirectConnection (same thread)
 * - AppController→MainWindow signals use Qt::QueuedConnection (cross-thread)
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

public:
    /**
     * @brief Set the plugin directory path (for testing)
     * @param path Directory path to scan for plugins
     */
    void setPluginDirectory(const QString &path);

    /**
     * @brief Get list of all available cameras from loaded plugins
     * @return List of camera identifiers
     */
    QStringList availableCameras() const;

    /**
     * @brief Get information about all loaded plugins
     * @return List of PluginInfo structures containing plugin metadata
     *
     * @see hasLoadedPlugins() to check if any plugins are loaded
     */
    QList<PluginInfo> loadedPlugins() const;

    /**
     * @brief Check if any plugins are loaded
     * @return true if plugins are available
     */
    bool hasPlugins() const;

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
     * @brief Get current camera state
     * @return CameraState enum value
     */
    CameraState state() const;

    /**
     * @brief Get current driver instance
     * @return Pointer to ICameraDriver, or nullptr
     */
    ICameraDriver *driver() const { return m_driver; }

    /**
     * @brief Validate all current parameters
     * @return true if all parameters are valid
     */
    bool validateParameters();

public slots:
    /**
     * @brief Scan for available camera driver plugins
     *
     * Discovers .dll files in the plugins/drivers directory and
     * loads valid ICameraDriver implementations.
     */
    void scanPlugins();

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
     * @brief Set a parameter value
     * @param name Parameter identifier
     * @param value New value
     * @return true if value was accepted
     */
    bool setParameter(const QString &name, const QVariant &value);

    /**
     * @brief Apply multiple parameters at once
     * @param params Map of parameters to apply
     * @return true if all parameters applied successfully
     */
    bool setParameters(const QVariantMap &params);

    /**
     * @brief Commit all staged parameter changes
     * @return true if commit succeeded
     */
    bool commitParameters();

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

    /**
     * @brief Emitted during plugin scan to report progress
     * @param current Current file index (1-based)
     * @param total Total number of files to scan
     * @param currentFile Absolute path of the file being processed
     */
    void pluginScanProgress(int current, int total, const QString &currentFile);

    /**
     * @brief Emitted when async connectCamera completes
     * @param cameraId Camera that was connected
     * @param success true if connected successfully
     * @param error Error message if failed
     */
    void connectCameraFinished(const QString &cameraId, bool success, const QString &error);

    /**
     * @brief Emitted when async disconnect completes
     * @param cameraId Camera that was disconnected
     */
    void disconnectCameraFinished(const QString &cameraId);

    /**
     * @brief Emitted when async commitParameters completes
     * @param success true if commit succeeded
     */
    void commitParametersFinished(bool success);

    /**
     * @brief Emitted when async setParameters completes
     * @param success true if all parameters were set successfully
     */
    void setParametersFinished(bool success);

private slots:
    void onDriverFrameReady(const QSharedPointer<QImage> &image,
                           quint64 timestamp,
                           int frameNumber,
                           const QString &cameraId,
                          const QVariantMap &parameters);
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

    const PluginInfo *findPluginForCamera(const QString &cameraId) const;
    bool loadPlugin(const QString &filePath);
    void unloadPlugin(const PluginInfo &info);
    void clearPlugins();

    // INI persistence helpers
    static QString getConfigDirectory();
    static QString getConfigPath(const QString &cameraId);
    static QString sanitizeFilenameComponent(const QString &name);
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
    QVariantMap m_pendingParameters;

    // Capture tracking
    int m_captureCount = 0;
    
    bool m_fisrtSetParameter = true;
};

#endif // APPCONTROLLER_H