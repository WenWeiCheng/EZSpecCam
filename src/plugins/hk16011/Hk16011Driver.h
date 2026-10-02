#pragma once

/**
 * @file Hk16011Driver.h
 * @brief ICameraDriver implementation for the HK16011 CCD spectrometer.
 *
 * Wraps the vendor C SDK (libHK16011, one public header `hk16011.h`) and
 * exposes the device's full parameter table through the EZSpecCam dynamic
 * parameter system.
 *
 * Threading model
 * ---------------
 * Every SDK call is issued from the Qt thread that owns this object. The SDK
 * delivers frames on its own internal reader thread through a C callback; that
 * callback does the minimum possible work (wrap the raw buffer in a QImage and
 * hand it to a mutex-protected queue) and never touches a QObject. A queued
 * invocation back onto this object's thread drains the queue and emits
 * frameReady(), so consumers only ever see frames on the Qt thread.
 *
 * Acquisition
 * -----------
 * The SDK's bounded fetch (`ACQ fetch <n>`) does not deliver a frame when
 * n == 1, and the very first acquisition after HK16011_Open is a cold start
 * that yields no frames. To work around both, the driver always starts a
 * *continuous* fetch (count 0) and stops it itself once the requested number
 * of frames has been emitted. That path is the only one that is reliable for
 * every capture count, including 1.
 */

#include "core/ICameraDriver.h"
#include "core/CameraTypes.h"

#include "hk16011.h"

#include <QObject>
#include <QRecursiveMutex>
#include <QSharedPointer>
#include <QStringList>
#include <QVariantMap>

#include <atomic>
#include <deque>

class Hk16011Driver : public ICameraDriver
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.ezspeccam.ICameraDriver" FILE "hk16011.json")
    Q_INTERFACES(ICameraDriver)

public:
    explicit Hk16011Driver(QObject *parent = nullptr);
    ~Hk16011Driver() override;

    // ——— ICameraDriver ———
    QStringList enumerate() override;
    bool connectToCamera(const QString &cameraId) override;
    void disconnectCamera() override;
    bool isConnected() const override;

    QStringList parameterNames() const override;
    ParameterDefinition parameter(const QString &name) const override;
    QVariant parameterValue(const QString &name) const override;
    bool setParameter(const QString &name, const QVariant &value,
                      QStringList *failedParameters = nullptr) override;
    bool setParameters(const QVariantMap &parameters,
                       QStringList *failedParameters = nullptr) override;
    bool validateParameters() override;
    bool commitParameters(QStringList *failedParameters = nullptr) override;

    bool startCapture(int captureCount = 0) override;
    void stopCapture(int timeoutMs = 5000) override;

    CameraState state() const override;
    QString driverVersion() const override;
    QString cameraId() const override;

private slots:
    /// Drains the frame queue on the Qt thread and emits frameReady().
    void deliverQueuedFrames();

private:
    /// One frame handed over from the SDK reader thread to the Qt thread.
    struct QueuedFrame
    {
        QSharedPointer<QImage> image;
        quint64 timestamp = 0;  ///< microseconds since epoch
        quint32 frameNumber = 0;
    };

    // ——— Setup ———
    /// Reads LISTPARAMS + every current value and fills the definition/value maps.
    void buildParameterTable();
    void clearParameterTable();

    // ——— Value conversion ———
    static QVariant toVariant(const HK16011_ValueStruct &value);
    static bool fromVariant(const ParameterDefinition &def, const QVariant &value,
                            HK16011_ValueStruct *out);
    static bool validateValue(const QVariant &value, const ParameterDefinition &def);

    // ——— Hardware ———
    QVariant readValue(const QString &name, bool *ok = nullptr) const;
    bool writeValue(const QString &name, const QVariant &value);
    /// Pushes every pending parameter to the device, accumulating failures.
    bool flushPendingParameters(QStringList *failedParameters);

    // ——— Capture plumbing ———
    static void onSdkFrame(const HK16011_FrameStruct *frame, void *user);
    void queueFrame(const HK16011_FrameStruct *frame);
    /// Stops the SDK reader. Must be called with m_mutex held.
    void finishCaptureLocked();

    // ——— Errors ———
    void reportError(CameraError::Code code, const QString &description,
                     CameraError::Severity severity = CameraError::Severity::Error,
                     const QStringList &failedParameters = QStringList());
    void reportSdkError(const QString &context, int code);
    static CameraError::Code mapErrorCode(int sdkCode);

    // ——— State ———
    HK16011_DeviceHandle *m_device = nullptr;
    std::atomic<CameraState> m_state{CameraState::Disconnected};
    QString m_connectedCameraId;

    /// Recursive because disconnectCamera() reaches stopCapture() while holding it.
    mutable QRecursiveMutex m_mutex;
    QMap<QString, ParameterDefinition> m_parameterDefinitions;
    QVariantMap m_parameters;        ///< last known values read from the device
    QVariantMap m_pendingParameters; ///< staged by setParameter(), applied on commit

    // ——— Capture ———
    std::atomic<bool> m_capturing{false};
    int m_captureCount = 0;                  ///< Qt thread only
    std::atomic<int> m_framesDelivered{0};
    std::atomic<int> m_frameNumber{0};

    QRecursiveMutex m_frameQueueMutex;
    std::deque<QueuedFrame> m_frameQueue;
    std::atomic<bool> m_deliveryScheduled{false};
};
