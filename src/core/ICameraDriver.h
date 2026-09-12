/**
 * @file ICameraDriver.h
 * @brief Simplified camera driver interface for EZSpecCam
 *
 * This interface defines the contract for all camera drivers in the EZSpecCam
 * application. It provides a clean abstraction over camera hardware with support
 * for discovery, connection management, parameter control, and frame capture.
 *
 * Key design decisions vs. original interface:
 * - Signal-based frame delivery (frameReady) replaces FrameBuffer + popFrame pattern
 * - Capture count is passed to startCapture() rather than stored as a property
 * - Timeout is passed to stopCapture() rather than stored as a property
 * - Reduced from 17+ virtual methods to ~10 for simpler driver implementations
 *
 * Drivers implement this interface and are loaded via Qt's plugin system.
 * Use Q_DECLARE_INTERFACE() at the bottom of this file for plugin registration.
 */

#pragma once

#include <QObject>
#include <QSharedPointer>
#include <QImage>
#include <QString>
#include <QVariant>
#include <QStringList>
#include "CameraTypes.h"

class ICameraDriver : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Construct an ICameraDriver with optional parent.
     * @param parent Optional parent QObject
     */
    explicit ICameraDriver(QObject *parent = nullptr) : QObject(parent) {}

    /**
     * @brief Virtual destructor for proper cleanup in derived classes.
     */
    virtual ~ICameraDriver() {}

    // ——— Discovery ———

    /**
     * @brief Enumerate all available cameras on the system.
     * @return QStringList of camera identifiers that can be passed to connectToCamera().
     *
     * This method scans the system for cameras and returns their unique
     * identifiers. The format of identifiers is driver-specific (e.g., device
     * paths, serial numbers, or friendly names).
     */
    virtual QStringList enumerate() = 0;

    // ——— Connection ———

    /**
     * @brief Establish connection to the specified camera.
     * @param cameraId Identifier obtained from enumerate().
     * @return true if connection succeeded, false otherwise.
     *
     * After successful connection, the driver should emit connectionChanged(true).
     * If connection fails, an errorOccurred signal should be emitted.
     */
    virtual bool connectToCamera(const QString &cameraId) = 0;

    /**
     * @brief Disconnect from the currently connected camera.
     *
     * This method cleanly shuts down any active capture session and releases
     * hardware resources. If not currently connected, this method does nothing.
     * After disconnection, connectionChanged(false) will be emitted.
     */
    virtual void disconnectCamera() = 0;

    /**
     * @brief Query whether a camera is currently connected.
     * @return true if connected, false otherwise.
     */
    virtual bool isConnected() const = 0;

    // ——— Parameters ———

    /**
     * @brief Get list of all parameter names exposed by this driver.
     * @return QStringList of parameter identifiers.
     *
     * These names can be used with parameter() and parameterValue() to
     * query or modify camera settings.
     */
    virtual QStringList parameterNames() const = 0;

    /**
     * @brief Get the full definition of a parameter.
     * @param name Parameter identifier from parameterNames().
     * @return ParameterDefinition struct with metadata (type, range, etc.).
     *
     * Returns a default-constructed ParameterDefinition if the parameter
     * does not exist.
     */
    virtual ParameterDefinition parameter(const QString &name) const = 0;

    /**
     * @brief Get the current value of a parameter.
     * @param name Parameter identifier.
     * @return Current value as QVariant, or QVariant::Invalid if not found.
     */
    virtual QVariant parameterValue(const QString &name) const = 0;

    /**
     * @brief Set a parameter to a new value.
     * @param name Parameter identifier.
     * @param value New value to assign.
     * @param failedParameters Optional output list; when non-null and the value is
     *        rejected, the parameter name is appended to this list.
     * @return true if the value was accepted, false otherwise.
     *
     * On rejection the driver emits errorOccurred carrying Severity::Warning with
     * the parameter name in failedParameters. Drivers MUST NOT emit Severity::Error
     * or higher from this path.
     * Changes may be staged until commitParameters() is called.
     */
    virtual bool setParameter(const QString &name, const QVariant &value,
                              QStringList *failedParameters = nullptr) = 0;

    /**
     * @brief Stage a batch of parameters at once.
     * @param parameters Map of name -> value pairs.
     * @param failedParameters Optional output list; when non-null, names that
     *        failed validation are appended (does not include parameters that
     *        passed validation but later fail at commit time).
     * @return true if all parameters were staged successfully.
     *
     * Drivers iterate the map, validate each entry against its constraints, and
     * collect failures into *failedParameters. On any rejection the driver emits
     * a single errorOccurred carrying Severity::Warning with the failed names in
     * failedParameters; the driver never emits Severity::Error or higher from
     * this path.
     */
    virtual bool setParameters(const QVariantMap &parameters,
                               QStringList *failedParameters = nullptr) = 0;

    /**
     * @brief Validate all current parameter values.
     * @return true if all parameters are within valid ranges, false otherwise.
     *
     * This checks all staged parameter changes without applying them.
     * Use this to validate before committing.
     */
    virtual bool validateParameters() = 0;

    /**
     * @brief Commit all staged parameter changes to the camera.
     * @param failedParameters Optional output list; when non-null, names that
     *        the hardware rejected are appended.
     * @return true if every staged parameter was applied successfully.
     *
     * Drivers drive every staged parameter; per-parameter hardware failures are
     * accumulated into *failedParameters (when non-null) rather than causing an
     * early return. A single errorOccurred carrying Severity::Warning is emitted
     * with the accumulated names in failedParameters when any parameter failed.
     * True hardware faults (SDK open failure, ROI validation crash, etc.) may
     * still emit Severity::Error and short-circuit. Severity::Fatal is never
     * emitted from this path.
     */
    virtual bool commitParameters(QStringList *failedParameters = nullptr) = 0;
    // ——— Capture ———

    /**
     * @brief Start capturing frames from the camera.
     * @param captureCount Number of frames to capture (0 = continuous until stopCapture).
     * @return true if capture started successfully, false otherwise.
     *
     * On success, captureStarted() signal is emitted. Frames are delivered
     * via the frameReady() signal. If captureCount > 0, capture will
     * automatically stop after that many frames and emit captureStopped().
     */
    virtual bool startCapture(int captureCount = 0) = 0;

    /**
     * @brief Stop an active capture session.
     * @param timeoutMs Maximum time to wait (in milliseconds) for capture to stop.
     *
     * This gracefully stops the capture, ensuring any in-progress frame
     * acquisition completes. After stopping, captureStopped() is emitted.
     */
    virtual void stopCapture(int timeoutMs = 5000) = 0;

    // ——— Driver Info ———

    /**
     * @brief Get the current state of the camera/driver.
     * @return CameraState enum value indicating current state.
     *
     * States typically include: Disconnected, Connecting, Connected,
     * Capturing, Error, etc.
     */
    virtual CameraState state() const = 0;

    /**
     * @brief Get the version string of this driver.
     * @return Version in "major.minor.patch" format.
     */
    virtual QString driverVersion() const = 0;

    /**
     * @brief Get the camera identifier for the connected camera.
     * @return Camera ID string, or empty string if not connected.
     */
    virtual QString cameraId() const = 0;

Q_SIGNALS:
    /**
     * @brief Emitted when a new frame is available from the camera.
     * @param image Shared pointer to the captured QImage.
     * @param timestamp Frame timestamp in microseconds since epoch.
     * @param frameNumber Sequential frame number for this camera session.
     * @param cameraId Identifier of the camera that produced this frame.
     *
     * This is the primary signal for frame delivery. Consumers connect
     * to this signal to receive captured frames. The image is shared
     * to avoid unnecessary copies while ensuring the image data remains
     * valid for the duration of slot execution.
     */
    void frameReady(const QSharedPointer<QImage> &image,
                    quint64 timestamp,
                    int frameNumber,
                    const QString &cameraId,
                    const QVariantMap &parameters);

    /**
     * @brief Emitted when capture begins.
     * @param cameraId Identifier of the camera that started capturing.
     */
    void captureStarted(const QString &cameraId);

    /**
     * @brief Emitted when capture ends (via stopCapture or after captureCount frames).
     * @param cameraId Identifier of the camera that stopped capturing.
     */
    void captureStopped(const QString &cameraId);

    /**
     * @brief Emitted when connection state changes.
     * @param connected true if connected, false if disconnected.
     * @param cameraId Identifier of the camera whose state changed.
     */
    void connectionChanged(bool connected, const QString &cameraId);

    /**
     * @brief Emitted when an error occurs in the driver or camera.
     * @param error CameraError struct with error code and optional message.
     */
    void errorOccurred(const CameraError &error);
};

Q_DECLARE_INTERFACE(ICameraDriver, "com.ezspeccam.ICameraDriver/1.0")