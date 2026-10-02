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
 * The device's own modes are used: 0 = live, 1 = single, >= 2 = burst of that
 * count. All three fetch the same way: a continuous `ACQ fetch 0` issued right
 * after the acquisition starts, so frames leave the device as they are exposed
 * instead of after the whole burst. A bounded `ACQ fetch <n>` is deliberately
 * not used — it is only accepted once all n frames are cached, which is why
 * asking for one meant waiting out the entire burst. It was also the only path
 * that returned every frame, back when a continuous fetch dropped the last
 * frame of a burst and nothing at all for a single; the firmware has since
 * fixed that (see AGENTS.md), so the distinction no longer holds.
 */

#include "core/ICameraDriver.h"
#include "core/CameraTypes.h"

#include "hk16011.h"

#include <QObject>
#include <QHash>
#include <QRecursiveMutex>
#include <QSharedPointer>
#include <QTimer>
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

    /// Watchdog for a bounded capture: ends it if the camera never delivers
    /// every frame the caller asked for.
    void checkCaptureTimeout();

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
    /// Drops parameters hk16011ParameterMetadata() does not list, then applies its
    /// category and unit to the ones that survive.
    void applyParameterMetadata();

    // ——— Value conversion ———
    static QVariant toVariant(const HK16011_ValueStruct &value);
    /// Encodes `value` into `out`. A String or Enumeration value is pointed at
    /// by `out->data.s.set`, so the bytes must live in `storage`, which the
    /// caller owns and keeps alive until the SDK call returns.
    bool fromVariant(const ParameterDefinition &def, const QVariant &value,
                     QByteArray *storage, HK16011_ValueStruct *out) const;
    static bool validateValue(const QVariant &value, const ParameterDefinition &def);
    /// Maps a wire token onto the label the GUI shows. Anything that is not a
    /// token of this parameter — a label, or a value of another type — passes
    /// through untouched.
    QVariant toShownValue(const ParameterDefinition &def, const QVariant &value) const;

    // ——— Hardware ———
    QVariant readValue(const QString &name, bool *ok = nullptr) const;
    bool writeValue(const QString &name, const QVariant &value);
    /// Pushes every pending parameter to the device, accumulating failures.
    bool flushPendingParameters(QStringList *failedParameters);

    // ——— Capture plumbing ———
    static void onSdkFrame(const HK16011_FrameStruct *frame, void *user);
    void queueFrame(const HK16011_FrameStruct *frame);
    /// Issues the continuous fetch (`ACQ fetch 0`). Must be called with m_mutex
    /// held.
    bool startFetchLocked();
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

    /// Enumeration parameters carry labels, but the device only speaks tokens.
    /// Both directions are kept so a value can be shown and still be sent:
    /// setParameter() normalizes token -> label, fromVariant() maps back.
    QMap<QString, QHash<QString, QString>> m_enumLabelByToken; ///< parameter -> token -> label
    QMap<QString, QHash<QString, QString>> m_enumTokenByLabel; ///< parameter -> label -> token

    // ——— Capture ———
    std::atomic<bool> m_capturing{false};
    int m_captureCount = 0;                  ///< Qt thread only
    qint64 m_captureDeadline = 0;            ///< epoch ms, Qt thread only
    QTimer *m_captureTimer = nullptr;
    std::atomic<int> m_framesDelivered{0};
    std::atomic<int> m_frameNumber{0};

    QRecursiveMutex m_frameQueueMutex;
    std::deque<QueuedFrame> m_frameQueue;
    std::atomic<bool> m_deliveryScheduled{false};
};
