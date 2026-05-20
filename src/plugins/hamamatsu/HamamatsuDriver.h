/**
 * @file HamamatsuDriver.h
 * @brief Hamamatsu DCAMSDK4 Camera Driver implementing ICameraDriver interface
 *
 * Driver for Hamamatsu scientific cameras using the DCAMSDK4 C API.
 * Discovers parameters dynamically at runtime via dcamprop_getnextid().
 * All parameter constraints are read from the SDK, not hardcoded.
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
#include <cstdint>
#include <qobject.h>
#include <vector>

// DCAMSDK4 headers
#include "dcamapi4.h"
#include "dcamprop.h"

/**
 * @class HamamatsuDriver
 * @brief Hamamatsu DCAMSDK4 camera driver implementation
 *
 * Communicates with Hamamatsu cameras via the DCAMSDK4 C API.
 * Supports dynamic property enumeration, async frame capture,
 * and thread-safe parameter access.
 *
 * @section SDKLifecycle SDK Lifecycle Management
 * Uses static reference counting to properly initialize/release the global SDK.
 *
 * @section Threading Threading Model
 * Frame capture runs on a dedicated QThread to avoid blocking the main thread.
 * All public methods are thread-safe via QRecursiveMutex.
 */
class HamamatsuDriver : public ICameraDriver {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.ezspeccam.ICameraDriver" FILE "hamamatsu.json")
    Q_INTERFACES(ICameraDriver)

public:
    /**
     * @brief Construct a new HamamatsuDriver instance
     * @param parent Optional parent QObject
     */
    explicit HamamatsuDriver(QObject *parent = nullptr);

    /**
     * @brief Destroy the HamamatsuDriver instance
     *
     * Ensures clean shutdown by disconnecting camera and releasing SDK resources.
     */
    ~HamamatsuDriver() override;

    //==========================================================================
    // ICameraDriver Interface Implementation
    //==========================================================================

    // ——— Discovery ———

    /**
     * @brief Enumerate all available Hamamatsu cameras
     * @return QStringList of camera identifiers (descriptive strings)
     *
     * Each identifier includes model name and serial number,
     * e.g. "C16091-10 (424QC101) on USB3"
     */
    QStringList enumerate() override;

    // ——— Connection ———

    /**
     * @brief Connect to a Hamamatsu camera
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
     *
     * For dynamic (isDynamic) and extrinsic (isExtrinsic) parameters,
     * this method re-queries the camera hardware to get the latest value.
     */
    QVariant parameterValue(const QString &name) const override;

    /**
     * @brief Set parameter value (stages it for commit)
     * @param name Parameter identifier
     * @param value New value
     * @return true if value was accepted
     *
     * Values are staged in pendingParameters until commitParameters() is called.
     * Read-only parameters are silently accepted without staging.
     */
    bool setParameter(const QString &name, const QVariant &value) override;

    /**
     * @brief Validate all pending parameter changes
     * @return true if all values are valid
     */
    bool validateParameters() override;

    /**
     * @brief Commit pending parameters to camera hardware
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

private slots:
    void onCaptureCompleted();

    void onCaptureLoop();

private:
    // ——— SDK Lifecycle ———

    /**
     * @brief Initialize the DCAMSDK4
     * @return true if initialization succeeded
     *
     * Uses reference counting to support multiple driver instances.
     */
    static bool initSdk();

    /**
     * @brief Release the DCAMSDK4
     *
     * Decrements reference count and uninitializes SDK when count reaches zero.
     */
    static void releaseSdk();

    /**
     * @brief Get DCAM device index from our camera identifier string
     * @param cameraId The camera ID from enumerate()
     * @return Device index, or -1 if not found
     */
    int deviceIndexFromId(const QString &cameraId) const;

    // ——— Property Enumeration ———

    /**
     * @brief Build a descriptive camera ID for the given device index
     * @param deviceIndex DCAM device index
     * @return Descriptive string like "C16091-10 (424QC101) on USB3"
     */
    QString buildCameraIdString(int deviceIndex) const;

    /**
     * @brief Enumerate all DCAM properties and build parameter definitions
     *
     * Iterates through all supported properties via dcamprop_getnextid(),
     * reads their attributes, categorizes them, and builds ParameterDefinition entries.
     * Excluded properties (SUBARRAY, TIMING, RECORD) are skipped.
     */
    void enumerateProperties();

    /**
     * @brief Build a ParameterDefinition from a DCAM property
     * @param iProp DCAM property ID
     * @param propName DCAM property name string
     * @param attr DCAM property attributes
     * @return ParameterDefinition, or invalid definition if property should be skipped
     */
    ParameterDefinition buildParameterDefinition(
        int32 iProp, const QString &propName, const DCAMPROP_ATTR &attr) const;

    /**
     * @brief Categorize a DCAM property into a ParameterCategory
     * @param propName DCAM property name
     * @param iProp DCAM property ID
     * @return ParameterCategory enum
     */
    ParameterCategory categorizeProperty(const QString &propName, int32 iProp) const;

    /**
     * @brief Map a DCAM property name to an ICameraDriver parameter name
     * @param dcamName DCAM property name string (e.g. "EXPOSURE TIME")
     * @return ICameraDriver parameter name (e.g. "exposure"), or empty if unmapped
     */
    QString mapPropertyName(const QString &dcamName) const;

    /**
     * @brief Map an ICameraDriver parameter name back to the DCAM property name
     * @param paramName ICameraDriver parameter name
     * @return DCAM property name, or empty if not found
     */
    QString reverseMapPropertyName(const QString &paramName) const;

    /**
     * @brief Check if a DCAM property is in the exclusion list
     * @param propName DCAM property name
     * @return true if property should be excluded
     */
    bool isExcluded(const QString &propName) const;

    // ——— DCAM Property I/O ———

    /**
     * @brief Get a double value for a DCAM property
     * @param iProp Property ID
     * @param value Output value
     * @return true if successful
     */
    bool getDcamPropertyValue(int32 iProp, double &value) const;

    /**
     * @brief Set a double value for a DCAM property
     * @param iProp Property ID
     * @param value Value to set
     * @return true if successful
     */
    bool setDcamPropertyValue(int32 iProp, double value);

    /**
     * @brief Convert DCAM error code to CameraError
     * @param err DCAMERR value
     * @param context Description of the operation that failed
     * @return CameraError struct
     */
    CameraError dcamErrToError(DCAMERR err, const QString &context) const;

    // ——— Capture Helpers ———

    /**
     * @brief Convert a DCAMBUF_FRAME to a QImage
     * @param frame Locked DCAM frame buffer
     * @return QSharedPointer to the converted QImage
     */
    QSharedPointer<QImage> convertFrameToImage(const DCAMBUF_FRAME &frame) const;

    //==========================================================================
    // Member Variables
    //==========================================================================

    // Thread safety
    mutable QRecursiveMutex m_mutex;

    // Connection state
    std::atomic<CameraState> m_state{CameraState::Disconnected};

    // DCAM SDK handles
    HDCAM m_hdcam = nullptr;
    HDCAMWAIT m_hwait = nullptr;

    // Camera identifiers
    QString m_connectedCameraId;
    int m_deviceIndex = -1;

    // Parameter system
    QMap<QString, ParameterDefinition> m_parameterDefinitions;
    QMap<QString, QVariant> m_parameters;
    QMap<QString, QVariant> m_pendingParameters;
    QMap<QString, int32> m_propertyIds;
    QMap<int32, QString> m_dcamPropNames;
    mutable QMap<QString, double> m_stringCollectionValueMap;  // Maps "propName::textValue" -> numeric value

    // Capture state
    QThread *m_captureThread = nullptr;
    std::atomic<bool> m_capturing{false};
    std::atomic<bool> m_resourcesHeld{false};
    std::atomic<int> m_captureCountTarget{0};
    std::atomic<int> m_framesCaptured{0};
    int m_imageWidth = 0;
    int m_imageHeight = 0;
    DCAM_PIXELTYPE m_pixelType{};

    // SDK reference counting (static, shared across instances)
    static std::atomic<bool> s_sdkInitialized;
    static int s_sdkRefCount;

    // Driver version
    static const char *const s_driverVersion;
};

// Helper: check DCAMERR for failure (matching SDK's inline failed() function)
inline bool dcamFailed(DCAMERR err)
{
    return static_cast<int>(err) < 0;
}
