/**
 * @file PicamDriver.h
 * @brief Teledyne Princeton Instruments PICam 5.x Camera Driver
 *
 * Driver for Princeton Instruments scientific cameras using the PICam 5.x SDK.
 * Implements ICameraDriver with runtime parameter discovery from the SDK.
 *
 * @see ICameraDriver
 * @see src/plugins/picam/AGENTS.md for parameter mapping details
 */

#pragma once

#include "core/ICameraDriver.h"
#include "core/CameraTypes.h"

#include <QObject>
#include <QMutex>
#include <QRecursiveMutex>
#include <QMap>
#include <QVariantMap>
#include <QThread>
#include <atomic>
#include <cstdint>

// PICam 5.x SDK
#include "picam.h"

/**
 * @class PicamDriver
 * @brief Princeton Instruments PICam 5.x camera driver
 *
 * Communicates with PI cameras via the PICam 5.x C API.
 * Discovers parameters dynamically at runtime via Picam_GetParameters().
 * ROI is handled as a composite decomposition layer.
 *
 * @section SDKLifecycle SDK Lifecycle Management
 * Uses static reference counting (same pattern as HamamatsuDriver).
 *
 * @section Threading Threading Model
 * Frame capture runs on a dedicated QThread.
 * All public methods are thread-safe via QRecursiveMutex.
 */
class PicamDriver : public ICameraDriver {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.ezspeccam.ICameraDriver" FILE "picam.json")
    Q_INTERFACES(ICameraDriver)

public:
    explicit PicamDriver(QObject *parent = nullptr);
    ~PicamDriver() override;

    //==========================================================================
    // ICameraDriver Interface Implementation
    //==========================================================================

    // ——— Discovery ———
    QStringList enumerate() override;

    // ——— Connection ———
    bool connectToCamera(const QString &cameraId) override;
    void disconnectCamera() override;
    bool isConnected() const override;

    // ——— Parameters ———
    QStringList parameterNames() const override;
    ParameterDefinition parameter(const QString &name) const override;
    QVariant parameterValue(const QString &name) const override;
    bool setParameter(const QString &name, const QVariant &value) override;
    bool validateParameters() override;
    bool commitParameters() override;

    // ——— Capture ———
    bool startCapture(int captureCount = 0) override;
    void stopCapture(int timeoutMs = 5000) override;

    // ——— Status ———
    CameraState state() const override;
    QString driverVersion() const override;
    QString cameraId() const override;

private:
    //==========================================================================
    // Internal Helpers
    //==========================================================================

    /**
     * @brief Initialize parameter definitions from the PICam SDK
     *
     * Calls Picam_GetParameters() to enumerate all available parameters,
     * builds ParameterDefinition entries with constraints and metadata,
     * and populates the parameter maps. ROI is decomposed into sub-params.
     */
    void initializeParameterDefinitions();

    /**
     * @brief Build a ParameterDefinition from a PicamParameter enum value
     * @param param Picam parameter enum
     * @return Configured ParameterDefinition
     */
    ParameterDefinition buildParameterDefinition(PicamParameter param);

    void initializeRoisSubParameters();

    /**
     * @brief Categorize a Picam parameter into a ParameterCategory
     * @param param Picam parameter enum
     * @return ParameterCategory enum
     */
    ParameterCategory categorizeParameter(PicamParameter param) const;

    /**
     * @brief Map a Picam parameter enum to an EZSpecCam parameter name
     * @param param Picam parameter enum
     * @return EZSpecCam parameter name string
     */
    QString mapParameterName(PicamParameter param) const;

    /**
     * @brief Get the PicamValueType for a stored parameter
     * @param name EZSpecCam parameter name
     * @return PicamValueType enum
     */
    PicamValueType getValueType(const QString &name) const;

    ParameterType mapValueType(PicamValueType vt) const;

    /**
     * @brief Sync all current parameter values from hardware
     *
     * Called after connectToCamera() and commitParameters(). For ROI,
     * this caches the composite PicamRois value for sub-param access.
     */
    void syncAllValuesFromHardware();

    // ——— ROI Sub-Parameter Helpers ———

    /**
     * @brief Check if a parameter name is an ROI sub-parameter
     * @param name Parameter name
     * @return true if it's a decomposed ROI sub-param
     */
    bool isRoiSubParam(const QString &name) const;

    /**
     * @brief Get a single field value from the cached PicamRois
     * @param name ROI sub-parameter name (e.g. "roi_x")
     * @return QVariant with the field value
     */
    QVariant getRoiSubValue(const QString &name) const;

    /**
     * @brief Set a single field value in the cached PicamRois
     * @param name ROI sub-parameter name
     * @param value New value
     */
    void setRoiSubValue(const QString &name, const QVariant &value);

    /**
     * @brief Assemble the pending ROI sub-params into a PicamRois struct
     * @param rois Output PicamRois struct (caller manages lifetime)
     */
    void assembleRois(PicamRois &rois) const;

    void onCaptureLoop();
    void processFrame(const PicamAvailableData& data);
    PicamError setEnumeratedParameter(PicamParameter param, const QString &value);
    QString mapParameterNameReverse(PicamParameter param) const;

    // ——— SDK Lifecycle ———

    static void initializeSDK();
    static void shutdownSDK();

    //==========================================================================
    // Member Variables
    //==========================================================================

    // Thread safety
    mutable QRecursiveMutex m_mutex;

    // Connection state
    std::atomic<CameraState> m_state{CameraState::Disconnected};
    CameraError m_lastError;

    // PICam handle
    PicamHandle m_handle = nullptr;

    // Camera identifiers
    QString m_connectedCameraId;
    PicamCameraID m_cameraID;
    QString m_driverVersion = "0.1.0";

    // Parameter system
    QMap<QString, ParameterDefinition> m_parameterDefinitions;
    QMap<QString, QVariant> m_parameters;        // Committed/active values
    QMap<QString, QVariant> m_pendingParameters;  // Staged values
    QMap<QString, PicamParameter> m_paramEnumMap; // name → PicamParameter enum
    QMap<QString, PicamValueType> m_paramTypeMap; // name → PicamValueType

    // ROI cache — composite PicamRois decomposed for sub-param access
    mutable PicamRoi m_cachedRoi;
    mutable bool m_roiDirty = false;

    // Capture state
    std::atomic<bool> m_capturing{false};
    std::atomic<int> m_captureCountTarget{0};
    std::atomic<int> m_framesCaptured{0};
    QThread* m_captureThread = nullptr;

    // SDK reference counting (static, shared across instances)
    static std::atomic<bool> s_sdkInitialized;
    static std::atomic<int> s_sdkRefCount;
};
