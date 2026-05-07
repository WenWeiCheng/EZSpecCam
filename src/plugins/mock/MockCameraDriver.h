/**
 * @file MockCameraDriver.h
 * @brief Mock Camera Driver implementing ICameraDriver directly
 *
 * A production-quality mock driver that generates synthetic frames for testing.
 * Does NOT use CameraDriverBase or ICameraHAL - directly implements ICameraDriver.
 *
 * Supports three virtual cameras: mock-001 (2048x2048), mock-002 (4096x4096), mock-003 (1024x1024).
 *
 * @see ICameraDriver
 */

#pragma once

#include "core/ICameraDriver.h"
#include "core/CameraTypes.h"

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QRecursiveMutex>
#include <QMap>
#include <QVariantMap>
#include <atomic>
#include <cstdint>

/**
 * @class MockCameraDriver
 * @brief Mock implementation of ICameraDriver for testing and development
 *
 * MockCameraDriver inherits directly from ICameraDriver and provides
 * virtual camera functionality with configurable image generation,
 * exposure simulation, and signal-based frame delivery.
 *
 * @section ConnectionState Connection State Management
 * Uses atomic variables for thread-safe state tracking.
 *
 * @section Performance Capabilities
 * - Exposure range: 1-10000 ms
 * - Gain range: 0-40 dB
 * - Max resolution: 4096x4096
 * - Bit depth: 16 bits
 * - Pixel binning: 1, 2, 4, 8 (camera-dependent)
 *
 * @section FrameGeneration Frame Generation
 * Generates synthetic frames using simple patterns:
 * - Pattern 0: Horizontal gradient
 * - Pattern 1: Random noise
 * - Pattern 2: Double-slit interference
 * - Pattern 3: Fast fill (diagonal gradient)
 *
 * @note All public methods are thread-safe.
 */
class MockCameraDriver : public ICameraDriver {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.ezspeccam.ICameraDriver" FILE "mock.json")

public:
    /**
     * @brief Construct a new MockCameraDriver instance
     * @param parent Optional parent QObject
     */
    explicit MockCameraDriver(QObject *parent = nullptr);

    /**
     * @brief Destroy the MockCameraDriver instance
     *
     * Ensures clean shutdown by stopping any ongoing capture.
     */
    ~MockCameraDriver() override;

    //==========================================================================
    // ICameraDriver Interface Implementation
    //==========================================================================

    /**
     * @brief Enumerate all available mock cameras
     * @return QStringList containing ["mock-001", "mock-002", "mock-003"]
     */
    QStringList enumerate() override;

    /**
     * @brief Connect to a mock camera
     * @param cameraId Camera identifier (e.g., "mock-001")
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
     * @brief Commit pending parameters to hardware
     * @return true if commit successful
     */
    bool commitParameters() override;

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
     * @brief Initialize parameter definitions for a camera model
     * @param cameraId Camera identifier
     */
    void initializeParameterDefinitions(const QString &cameraId);

    /**
     * @brief Generate a synthetic frame
     * @return QImage in Format_Grayscale16
     */
    QImage generateFrame();

    /**
     * @brief Generate horizontal gradient pattern
     */
    QImage generateGradientImage(int width, int height);

    /**
     * @brief Generate random noise pattern
     */
    QImage generateNoiseImage(int width, int height);

    /**
     * @brief Generate double-slit interference pattern
     */
    QImage generateDoubleSlitInterferenceImage(int width, int height);

    /**
     * @brief Generate fast fill pattern (diagonal gradient)
     */
    QImage generateFastFillImage(int width, int height);

    /**
     * @brief Validate value against parameter constraints
     */
    bool validateValue(const QVariant &value, const ParameterDefinition &def) const;

    /**
     * @brief Update simulated temperatures
     */
    void updateTemperatures();

    /**
     * @brief Capture loop worker (called by timer)
     */
    void onCaptureTimer();

    //==========================================================================
    // Private Members
    //==========================================================================

    /// Camera state
    std::atomic<CameraState> m_state{CameraState::Disconnected};

    /// Connected camera ID
    QString m_connectedCameraId;

    /// Current parameter values
    QVariantMap m_parameters;

    /// Parameter definitions
    QMap<QString, ParameterDefinition> m_parameterDefinitions;

    /// Pending parameters (staged before commit)
    QVariantMap m_pendingParameters;

    /// Whether capture is in progress
    std::atomic<bool> m_capturing{false};

    /// Capture count (0 = continuous)
    int m_captureCount = 0;

    /// Frames captured so far in current session
    std::atomic<int> m_framesAcquired{0};

    /// Current frame number
    std::atomic<int> m_frameNumber{0};

    /// Current gain value (for image generation)
    double m_gain = 1.0;

    /// Current pattern type (0=gradient, 1=noise, 2=interference, 3=fastfill)
    int m_patternType = 0;

    /// Current sensor temperature
    double m_currentSensorTemp = 25.0;

    /// Current heatsink temperature
    double m_currentHeatsinkTemp = 25.0;

    /// Mutex for thread-safe access (recursive for convenience)
    mutable QRecursiveMutex m_mutex;

    /// Flag indicating auto-stop (from onCaptureTimer) vs manual stop
    bool m_autoStop = false;

    /// Frame generation timer
    QTimer *m_captureTimer = nullptr;

    /// Last error
    CameraError m_lastError;
};