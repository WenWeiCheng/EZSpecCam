/**
 * @file QHYCCDDriver.h
 * @brief QHYCCD Camera Driver implementing ICameraDriver interface
 *
 * Driver for QHYCCD cameras using the official QHYCCD SDK.
 * Provides state management and async data fetching without blocking the main thread.
 *
 * @see ICameraDriver
 */

#pragma once

#include "core/ICameraDriver.h"
#include "core/CameraTypes.h"

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QRecursiveMutex>
#include <QMap>
#include <QVariantMap>
#include <atomic>
#include <qtypes.h>
#include <vector>
#include <cstdint>

// QHYCCD SDK headers
#include "qhyccd.h"
#include "qhyccderr.h"
#include "qhyccdstruct.h"
#include "qhyccdcamdef.h"

/**
 * @class QHYCCDDriver
 * @brief QHYCCD camera driver implementation
 *
 * QHYCCDDriver provides a bridge between the QHYCCD SDK and the ICameraDriver
 * interface. It manages SDK initialization, camera connection, parameter control,
 * and asynchronous frame capture.
 *
 * @section SDKLifecycle SDK Lifecycle Management
 * Uses static reference counting to properly initialize/release the global SDK.
 * Multiple cameras can be used simultaneously with shared SDK initialization.
 *
 * @section Threading Threading Model
 * Frame capture runs on a dedicated internal thread to avoid blocking the main thread.
 * All public methods are thread-safe via QRecursiveMutex.
 *
 * @note All public methods are thread-safe.
 */
class QHYCCDDriver : public ICameraDriver {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.ezspeccam.ICameraDriver" FILE "qhyccd.json")
    Q_INTERFACES(ICameraDriver)

public:
    /**
     * @brief Construct a new QHYCCDDriver instance
     * @param parent Optional parent QObject
     */
    explicit QHYCCDDriver(QObject *parent = nullptr);

    /**
     * @brief Destroy the QHYCCDDriver instance
     *
     * Ensures clean shutdown by disconnecting camera and releasing SDK resources.
     */
    ~QHYCCDDriver() override;

    //==========================================================================
    // ICameraDriver Interface Implementation
    //==========================================================================

    // ——— Discovery ———

    /**
     * @brief Enumerate all available QHYCCD cameras
     * @return QStringList of camera identifiers
     */
    QStringList enumerate() override;

    // ——— Connection ———

    /**
     * @brief Connect to a QHYCCD camera
     * @param cameraId Camera identifier from enumerate()
     * @return true if connection successful, false otherwise
     */
    bool connectToCamera(const QString &cameraId) override;

    /**
     * @brief Disconnect from the current camera
     */
    void disconnectCamera() override;

    /**
     * @brief Check connection state
     * @return true if connected, false otherwise
     */
    bool isConnected() const override;

    // ——— Parameters ———

    /**
     * @brief Get list of available parameter names
     * @return List of parameter identifiers
     */
    QStringList parameterNames() const override;

    /**
     * @brief Get parameter definition
     * @param name Parameter identifier
     * @return Parameter definition structure
     */
    ParameterDefinition parameter(const QString &name) const override;

    /**
     * @brief Get current parameter value
     * @param name Parameter identifier
     * @return Current value as QVariant
     */
    QVariant parameterValue(const QString &name) const override;

    /**
     * @brief Set parameter value
     * @param name Parameter identifier
     * @param value New value
     * @return true if value was accepted
     */
    bool setParameter(const QString &name, const QVariant &value) override;

    /**
     * @brief Validate all pending parameter changes
     * @return true if all values are valid
     */
    bool validateParameters() override;

    /**
     * @brief Commit pending parameters to camera
     * @return true if commit successful
     */
    bool commitParameters() override;

    // ——— Capture ———

    /**
     * @brief Start capture operation
     * @param captureCount Number of frames to capture (0 = continuous)
     * @return true if capture started successfully
     */
    bool startCapture(int captureCount = 0) override;

    /**
     * @brief Stop capture operation
     * @param timeoutMs Maximum time to wait for graceful stop
     */
    void stopCapture(int timeoutMs = 5000) override;

    // ——— State/Info ———

    /**
     * @brief Get current camera/driver state
     * @return CameraState enum value
     */
    CameraState state() const override;

    /**
     * @brief Get driver version string
     * @return Version in "major.minor.patch" format
     */
    QString driverVersion() const override;

    /**
     * @brief Get camera identifier for current connection
     * @return Camera ID string, or empty if not connected
     */
    QString cameraId() const override;

private:
    //==========================================================================
    // Private Methods
    //==========================================================================

    /**
     * @brief Initialize the QHYCCD SDK (reference-counted)
     * @return true if SDK initialized or already initialized
     */
    bool initSdk();

    /**
     * @brief Release the QHYCCD SDK (reference-counted)
     */
    void releaseSdk();

    /**
     * @brief Initialize parameter definitions for the connected camera
     */
    void initializeParameterDefinitions();

    /**
     * @brief Convert raw buffer to QImage
     */
    QImage convertBufferToImage(uint32_t width, uint32_t height, uint32_t bpp, uint32_t channels);

    /**
     * @brief Capture loop worker (runs on internal thread)
     */
    void captureLoop();

    //==========================================================================
    // Private Members
    //==========================================================================

    // ——— SDK lifecycle (static) ———

    /// SDK initialization flag
    static std::atomic<bool> s_sdkInitialized;

    /// SDK reference count for lifecycle management
    static int s_sdkRefCount;

    // ——— Capture thread ———

    /// Dedicated thread for frame capture
    QThread *m_captureThread = nullptr;

    /// Flag indicating capture is in progress
    std::atomic<bool> m_captureRunning{false};

    /// Number of frames acquired in current session
    std::atomic<int> m_framesAcquired{0};

    // ——— State ———

    /// Current camera/driver state
    std::atomic<CameraState> m_state{CameraState::Disconnected};

    /// Connected camera identifier
    QString m_connectedCameraId;

    /// Opaque SDK camera handle
    qhyccd_handle *m_cameraHandle = nullptr;

    /// Connection state
    std::atomic<bool> m_connected{false};

    /// Number of frames to capture (0 = continuous)
    std::atomic<int> m_captureCount{0};

    // ——— Parameters ———

    /// Parameter definitions
    QMap<QString, ParameterDefinition> m_parameterDefinitions;

    /// Current parameter values (mutable for caching in const methods)
    mutable QVariantMap m_parameters;

    /// Pending parameters (staged before commit)
    QVariantMap m_pendingParameters;

    // ——— Mutex ———

    /// Thread-safe access to driver state
    mutable QRecursiveMutex m_mutex;

    // ——— Camera info ———

    /// Camera model name
    QString m_cameraModel;
    QStringList m_readModeNames;

    /// Image dimensions
    uint32_t m_imageWidth = 0;
    uint32_t m_imageHeight = 0;
    uint32_t m_imageBytes = 0;
    uint32_t m_effectiveStartX = 0;
    uint32_t m_effectiveStartY = 0;
    uint32_t m_effectiveWidth = 0;
    uint32_t m_effectiveHeight = 0;
    double m_chipWidth = 0;
    double m_chipHeight = 0;
    double m_pixelWidth = 0;
    double m_pixelHeight = 0;

    // ——— Buffer ———

    /// Frame data buffer
    std::vector<uint8_t> m_frameBuffer;

    /// Buffer size in bytes
    uint32_t m_bufferSize = 0;
};