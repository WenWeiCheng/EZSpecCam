#include "Hk16011Driver.h"
#include "gui/DebugMacros.h"

#include "hk16011.h"

#include <libusb-1.0/libusb.h>

#include <QDateTime>
#include <QSet>
#include <QMetaObject>
#include <QTimer>
#include <QMutexLocker>
#include <QtEndian>

#include <cstring>

Q_LOGGING_CATEGORY(parameterCategory, "Parameter")
Q_LOGGING_CATEGORY(cameraCategory, "Camera")
Q_LOGGING_CATEGORY(configCategory, "Config")
Q_LOGGING_CATEGORY(displayCategory, "Display")
Q_LOGGING_CATEGORY(captureCategory, "Capture")
Q_LOGGING_CATEGORY(driverCategory, "Driver")

namespace {

/**
 * Parameters that define the frame geometry the SDK's reader re-derives for
 * every frame. Changing one of them while others in the same batch fail would
 * leave the reader and the device disagreeing about frame size, so a batch
 * containing a failing geometry parameter is rejected as a whole.
 */
const QSet<QString> &hk16011CriticalParameters()
{
    static const QSet<QString> set = {
        QStringLiteral("image_width"),
        QStringLiteral("image_height"),
        QStringLiteral("read_mode"),
    };
    return set;
}

/// The camera id the driver reports, matching HK16011_GetDeviceId()'s format:
/// a lowercase model prefix followed by uppercase hex VID and PID.
QString hk16011CameraId()
{
    const QString vid = QStringLiteral("%1")
                            .arg(HK16011_USB_VENDOR_ID, 4, 16, QLatin1Char('0'))
                            .toUpper();
    const QString pid = QStringLiteral("%1")
                            .arg(HK16011_DEFAULT_PID, 4, 16, QLatin1Char('0'))
                            .toUpper();
    return QStringLiteral("hk16011:%1:%2").arg(vid, pid);
}

/// Readiness poll cadence while waiting for a burst to be cached.
const int kCapturePollMs = 20;

/// How long a bounded capture may wait for its frames to be cached before the
/// driver gives up. A 20-frame burst at 1024x64 needs several seconds.
const qint64 kCaptureTimeoutMs = 20000;

ParameterCategory mapCategory(HK16011_CategoryEnum category, bool readOnly)
{
    // EZSpecCam defines Info as "read-only informational parameters", so every
    // read-only device parameter belongs there regardless of how the firmware
    // grouped it. The vendor grouping is only used for writable parameters.
    if (readOnly) {
        return ParameterCategory::Info;
    }
    switch (category) {
    case HK16011_Category_Cooling:
        return ParameterCategory::Cooling;
    case HK16011_Category_Info:
        return ParameterCategory::Info;
    case HK16011_Category_Advanced:
        return ParameterCategory::Advanced;
    case HK16011_Category_Debug:
        return ParameterCategory::Debug;
    case HK16011_Category_Core:
    default:
        return ParameterCategory::Core;
    }
}

} // namespace

//==============================================================================
// Construction
//==============================================================================

Hk16011Driver::Hk16011Driver(QObject *parent)
    : ICameraDriver(parent)
{
    m_captureTimer = new QTimer(this);
    m_captureTimer->setInterval(kCapturePollMs);
    connect(m_captureTimer, &QTimer::timeout, this, &Hk16011Driver::pollCapture);
}

Hk16011Driver::~Hk16011Driver()
{
    // Never emit from the destructor: the GUI may already be tearing down.
    if (m_capturing.load()) {
        m_capturing.store(false);
        if (m_captureTimer) {
            m_captureTimer->stop();
        }
        if (m_device) {
            HK16011_AbortFetch(m_device);
            HK16011_AbortCapture(m_device);
        }
    }
    if (m_device) {
        HK16011_SetFrameCallback(m_device, nullptr, nullptr);
        HK16011_Close(m_device);
        m_device = nullptr;
    }
}

//==============================================================================
// Errors
//==============================================================================

CameraError::Code Hk16011Driver::mapErrorCode(int sdkCode)
{
    switch (sdkCode) {
    case HK16011_ErrorCode_InvalidArg:
        return CameraError::Code::InvalidParameter;
    case HK16011_ErrorCode_UnknownParam:
        return CameraError::Code::InvalidParameter;
    case HK16011_ErrorCode_InvalidValue:
        return CameraError::Code::ValueOutOfRange;
    case HK16011_ErrorCode_NotWritable:
        return CameraError::Code::InvalidParameter;
    case HK16011_ErrorCode_DeviceBusy:
        return CameraError::Code::StateInvalid;
    case HK16011_ErrorCode_NoDevice:
    case HK16011_ErrorCode_OpenFailed:
    case HK16011_ErrorCode_ClaimFailed:
    case HK16011_ErrorCode_SerialOpen:
    case HK16011_ErrorCode_SerialIo:
        return CameraError::Code::ConnectionFailed;
    case HK16011_ErrorCode_UsbTransfer:
    case HK16011_ErrorCode_UsbTimeout:
    case HK16011_ErrorCode_FirmwareError:
        return CameraError::Code::CommunicationError;
    case HK16011_ErrorCode_Timeout:
        return CameraError::Code::Timeout;
    case HK16011_ErrorCode_Busy:
        return CameraError::Code::StateInvalid;
    default:
        return CameraError::Code::DriverError;
    }
}

void Hk16011Driver::reportError(CameraError::Code code, const QString &description,
                                CameraError::Severity severity,
                                const QStringList &failedParameters)
{
    emit errorOccurred(CameraError::makeError(code, description, severity, failedParameters));
}

void Hk16011Driver::reportSdkError(const QString &context, int code)
{
    const QString text = QString::fromLatin1(HK16011_ErrorCodeToString(
        static_cast<HK16011_ErrorCodeEnum>(code)));
    DRIVER_DEBUG << context << "failed:" << text << "(" << code << ")";
    reportError(mapErrorCode(code), QStringLiteral("%1: %2").arg(context, text));
}

//==============================================================================
// Discovery
//==============================================================================

QStringList Hk16011Driver::enumerate()
{
    // HK16011_Open() would work here, and it does not hold the UART
    // exclusively. The reason to probe with libusb instead is cost: Open
    // spends ~365 ms, almost all of it in the 35 UART round trips it uses to
    // prime the parameter cache (LISTPARAMS plus one GETINFO per parameter at
    // 115200 baud), against ~1 ms for a descriptor probe.
    //
    // enumerate() runs on every scan — at start-up and again on each "Scan
    // Plugins" click — and AppController (which owns those scans) also routes
    // frameReady() from the driver on the same thread. Blocking it for 365 ms
    // per plugin would stall frame delivery, and the SDK's frame queue is only
    // four frames deep, so a scan during a live capture would drop frames.
    libusb_context *context = nullptr;
    if (libusb_init(&context) != 0) {
        // Cannot tell; let connectToCamera() surface the real failure.
        return QStringList{ hk16011CameraId() };
    }

    libusb_device **devices = nullptr;
    const ssize_t count = libusb_get_device_list(context, &devices);
    bool found = false;
    for (ssize_t i = 0; i < count && !found; ++i) {
        struct libusb_device_descriptor descriptor;
        if (libusb_get_device_descriptor(devices[i], &descriptor) != 0) {
            continue;
        }
        if (descriptor.idVendor == HK16011_USB_VENDOR_ID
            && descriptor.idProduct == HK16011_DEFAULT_PID) {
            found = true;
        }
    }

    if (devices) {
        libusb_free_device_list(devices, 1);
    }
    libusb_exit(context);

    return found ? QStringList{ hk16011CameraId() } : QStringList();
}

//==============================================================================
// Connection
//==============================================================================

bool Hk16011Driver::connectToCamera(const QString &cameraId)
{
    QMutexLocker locker(&m_mutex);

    if (cameraId != hk16011CameraId()) {
        reportError(CameraError::Code::InvalidParameter,
                    QStringLiteral("Invalid camera ID: %1").arg(cameraId));
        return false;
    }

    if (m_state.load() == CameraState::Connected) {
        disconnectCamera();
    }

    m_state.store(CameraState::Connecting);

    HK16011_DeviceHandle *device = nullptr;
    const HK16011_ErrorCodeEnum rc = HK16011_Open(&device);
    if (rc != HK16011_OK || device == nullptr) {
        m_state.store(CameraState::Disconnected);
        reportSdkError(QStringLiteral("HK16011_Open"), rc);
        return false;
    }
    m_device = device;
    m_connectedCameraId = cameraId;

    buildParameterTable();

    // Registering the callback is what spawns the SDK's reader thread. The
    // driver only ever uses continuous fetches, so push mode is always enough.
    const HK16011_ErrorCodeEnum cbRc =
        HK16011_SetFrameCallback(m_device, &Hk16011Driver::onSdkFrame, this);
    if (cbRc != HK16011_OK) {
        reportSdkError(QStringLiteral("HK16011_SetFrameCallback"), cbRc);
        HK16011_Close(m_device);
        m_device = nullptr;
        m_connectedCameraId.clear();
        clearParameterTable();
        m_state.store(CameraState::Disconnected);
        return false;
    }

    m_frameNumber.store(0);
    m_framesDelivered.store(0);
    m_state.store(CameraState::Connected);

    DRIVER_DEBUG << "connected to" << cameraId << "with" << m_parameterDefinitions.size()
                  << "parameters";
    emit connectionChanged(true, cameraId);
    return true;
}

void Hk16011Driver::disconnectCamera()
{
    QMutexLocker locker(&m_mutex);

    if (m_state.load() == CameraState::Disconnected) {
        return;
    }

    stopCapture(100);

    if (m_device) {
        HK16011_SetFrameCallback(m_device, nullptr, nullptr);
        HK16011_Close(m_device);
        m_device = nullptr;
    }

    clearParameterTable();

    const QString oldCameraId = m_connectedCameraId;
    m_connectedCameraId.clear();
    m_state.store(CameraState::Disconnected);

    emit connectionChanged(false, oldCameraId);
}

bool Hk16011Driver::isConnected() const
{
    return m_state.load() == CameraState::Connected;
}

CameraState Hk16011Driver::state() const
{
    return m_state.load();
}

QString Hk16011Driver::driverVersion() const
{
    return QString::fromLatin1(HK16011_DriverVersion());
}

QString Hk16011Driver::cameraId() const
{
    return m_connectedCameraId;
}

//==============================================================================
// Parameter table
//==============================================================================

void Hk16011Driver::clearParameterTable()
{
    m_parameterDefinitions.clear();
    m_parameters.clear();
    m_pendingParameters.clear();
}

void Hk16011Driver::buildParameterTable()
{
    clearParameterTable();
    if (!m_device) {
        return;
    }

    HK16011_ParamDefStruct *defs = nullptr;
    size_t count = 0;
    const HK16011_ErrorCodeEnum rc = HK16011_ListParams(m_device, &defs, &count);
    if (rc != HK16011_OK) {
        reportSdkError(QStringLiteral("HK16011_ListParams"), rc);
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        const HK16011_ParamDefStruct &src = defs[i];
        const QString name = QString::fromLatin1(src.name);
        if (name.isEmpty()) {
            continue;
        }

        const bool readOnly = src.read_only != 0;

        ParameterDefinition def;
        def.name = name;
        def.displayName = QString::fromLatin1(src.display_name).trimmed();
        if (def.displayName.isEmpty()) {
            def.displayName = name;
        }
        def.description = QString::fromLatin1(src.description).trimmed();
        if (def.description.isEmpty()) {
            def.description = def.displayName;
        }
        def.category = mapCategory(src.category, readOnly);
        def.isReadOnly = readOnly;
        def.order = static_cast<float>(i);

        const QString unit = QString::fromLatin1(src.unit).trimmed();
        if (!unit.isEmpty()) {
            def.constraint.unit = QStringList{ unit };
        }

        switch (src.type) {
        case HK16011_ValueType_Int:
            def.type = ParameterType::IntRange;
            def.constraint.minValue = static_cast<double>(src.min_value.data.i);
            def.constraint.maxValue = static_cast<double>(src.max_value.data.i);
            def.constraint.step = static_cast<double>(src.step.data.i);
            break;
        case HK16011_ValueType_Float:
            def.type = ParameterType::FloatRange;
            def.constraint.minValue = src.min_value.data.d;
            def.constraint.maxValue = src.max_value.data.d;
            def.constraint.step = src.step.data.d;
            break;
        case HK16011_ValueType_Bool:
            def.type = ParameterType::Boolean;
            break;
        case HK16011_ValueType_Enumeration: {
            def.type = ParameterType::StringCollection;
            // The wire token is the canonical identifier and is what SETPARAM
            // expects, so it is what goes into validValues. The friendlier
            // firmware label is folded into the description instead.
            QStringList labels;
            const size_t itemCount =
                qMin(src.valid_item_count, static_cast<size_t>(HK16011_PARAM_ITEMS_MAX));
            for (size_t j = 0; j < itemCount; ++j) {
                const char *token = src.valid_items[j].data.s.set;
                const char *label = src.valid_items[j].data.s.label;
                if (!token) {
                    continue;
                }
                def.constraint.validValues.append(QString::fromLatin1(token));
                if (label) {
                    labels.append(QString::fromLatin1(label));
                }
            }
            if (!labels.isEmpty()) {
                def.description += QStringLiteral(" (%1)").arg(labels.join(QStringLiteral(", ")));
            }
            break;
        }
        case HK16011_ValueType_String:
        default:
            def.type = ParameterType::String;
            break;
        }

        def.defaultValue = toVariant(src.default_value);
        m_parameterDefinitions.insert(name, def);
    }

    HK16011_FreeParamList(defs);

    // Read the live value of every parameter. This also lets us spot the
    // telemetry the firmware reports but does not actually implement.
    for (auto it = m_parameterDefinitions.constBegin();
         it != m_parameterDefinitions.constEnd(); ++it) {
        bool ok = false;
        const QVariant value = readValue(it.key(), &ok);
        if (ok) {
            m_parameters.insert(it.key(), value);
        }
    }

    // The firmware's GETINFO reply carries only "<min>:<max>:<step>" — it never
    // reports a default, so the SDK hands us an Invalid value for every
    // parameter. ParameterDefinition::isValid() rejects a null default on
    // writable parameters, so fall back to the value the camera is actually
    // running at, then to the bottom of the range.
    for (auto it = m_parameterDefinitions.begin(); it != m_parameterDefinitions.end(); ++it) {
        ParameterDefinition &def = it.value();
        if (def.defaultValue.isValid()) {
            continue;
        }
        const QVariant current = m_parameters.value(def.name);
        if (current.isValid()) {
            def.defaultValue = current;
        } else if (def.type == ParameterType::IntRange
                   || def.type == ParameterType::FloatRange) {
            def.defaultValue = def.constraint.minValue;
        } else if (def.type == ParameterType::StringCollection) {
            def.defaultValue = def.constraint.validValues.isEmpty()
                ? QVariant(QString())
                : def.constraint.validValues.first();
        } else if (def.type == ParameterType::String) {
            def.defaultValue = QString();
        } else if (def.type == ParameterType::Boolean) {
            def.defaultValue = false;
        }
    }

    // A read-only parameter whose live reading falls outside the range the
    // device itself declares is a firmware placeholder, not a measurement.
    // Say so in the description so the GUI does not present it as real data.
    const QString placeholderNote = QStringLiteral(
        " [not implemented: the firmware reports a placeholder value for this "
        "telemetry, not a real measurement]");

    for (auto it = m_parameterDefinitions.begin(); it != m_parameterDefinitions.end(); ++it) {
        const ParameterDefinition &def = it.value();
        if (!def.isReadOnly
            || (def.type != ParameterType::IntRange
                && def.type != ParameterType::FloatRange)) {
            continue;
        }
        const QVariant value = m_parameters.value(def.name);
        if (!value.isValid()) {
            continue;
        }
        const double v = value.toDouble();
        if (v >= def.constraint.minValue && v <= def.constraint.maxValue) {
            continue;
        }
        ParameterDefinition annotated = def;
        annotated.description += placeholderNote;
        it.value() = annotated;
    }
}

//==============================================================================
// Value conversion
//==============================================================================

QVariant Hk16011Driver::toVariant(const HK16011_ValueStruct &value)
{
    // The union is only valid for the member matching `type`; reading another
    // member reinterprets the bits (an int read as a pointer, say) and would
    // hand back garbage.
    switch (value.type) {
    case HK16011_ValueType_Int:
        return QVariant::fromValue(static_cast<qlonglong>(value.data.i));
    case HK16011_ValueType_Float:
        return QVariant(value.data.d);
    case HK16011_ValueType_Bool:
        return QVariant(value.data.b != 0);
    case HK16011_ValueType_String:
    case HK16011_ValueType_Enumeration:
        return value.data.s.set ? QVariant(QString::fromLatin1(value.data.s.set))
                                : QVariant(QString());
    case HK16011_ValueType_Invalid:
    default:
        return QVariant();
    }
}

bool Hk16011Driver::fromVariant(const ParameterDefinition &def, const QVariant &value,
                                HK16011_ValueStruct *out)
{
    std::memset(out, 0, sizeof(*out));
    switch (def.type) {
    case ParameterType::IntRange:
        out->type = HK16011_ValueType_Int;
        out->data.i = value.toLongLong();
        return true;
    case ParameterType::FloatRange:
        out->type = HK16011_ValueType_Float;
        out->data.d = value.toDouble();
        return true;
    case ParameterType::Boolean:
        out->type = HK16011_ValueType_Bool;
        out->data.b = value.toBool() ? 1 : 0;
        return true;
    case ParameterType::String:
    case ParameterType::StringCollection: {
        // SETPARAM wants the bare wire token with no label attached.
        const QByteArray token = value.toString().toLatin1();
        out->type = def.type == ParameterType::String ? HK16011_ValueType_String
                                                       : HK16011_ValueType_Enumeration;
        out->data.s.set = token.constData();
        out->data.s.label = nullptr;
        return true;
    }
    default:
        return false;
    }
}

bool Hk16011Driver::validateValue(const QVariant &value, const ParameterDefinition &def)
{
    return ::validate(value, def.constraint, def.type);
}

//==============================================================================
// Parameters
//==============================================================================

QStringList Hk16011Driver::parameterNames() const
{
    QMutexLocker locker(&m_mutex);
    return m_parameterDefinitions.keys();
}

ParameterDefinition Hk16011Driver::parameter(const QString &name) const
{
    QMutexLocker locker(&m_mutex);
    return m_parameterDefinitions.value(name);
}

QVariant Hk16011Driver::parameterValue(const QString &name) const
{
    QMutexLocker locker(&m_mutex);
    if (m_pendingParameters.contains(name)) {
        return m_pendingParameters.value(name);
    }
    return m_parameters.value(name);
}

QVariant Hk16011Driver::readValue(const QString &name, bool *ok) const
{
    if (ok) {
        *ok = false;
    }
    if (!m_device) {
        return QVariant();
    }

    HK16011_ValueStruct value;
    std::memset(&value, 0, sizeof(value));
    const QByteArray utf8Name = name.toLatin1();
    const HK16011_ErrorCodeEnum rc =
        HK16011_GetParamValue(m_device, utf8Name.constData(), &value);
    if (rc != HK16011_OK) {
        return QVariant();
    }

    const QVariant result = toVariant(value);
    // Only String / Enumeration results own heap memory.
    if (value.type == HK16011_ValueType_String
        || value.type == HK16011_ValueType_Enumeration) {
        HK16011_FreeValue(&value);
    }
    if (ok) {
        *ok = true;
    }
    return result;
}

bool Hk16011Driver::writeValue(const QString &name, const QVariant &value)
{
    if (!m_device) {
        return false;
    }
    const ParameterDefinition def = m_parameterDefinitions.value(name);
    HK16011_ValueStruct sdkValue;
    if (!fromVariant(def, value, &sdkValue)) {
        return false;
    }
    const QByteArray utf8Name = name.toLatin1();
    const HK16011_ErrorCodeEnum rc =
        HK16011_SetParamValue(m_device, utf8Name.constData(), &sdkValue);
    return rc == HK16011_OK;
}

bool Hk16011Driver::setParameter(const QString &name, const QVariant &value,
                                 QStringList *failedParameters)
{
    QMutexLocker locker(&m_mutex);

    if (!m_parameterDefinitions.contains(name)) {
        if (failedParameters) {
            failedParameters->append(name);
        }
        reportError(CameraError::Code::InvalidParameter,
                    QStringLiteral("Unknown parameter: %1").arg(name),
                    CameraError::Severity::Warning, QStringList{ name });
        return false;
    }

    const ParameterDefinition def = m_parameterDefinitions.value(name);
    if (def.isReadOnly) {
        // Read-only parameters are accepted and ignored, matching the other
        // drivers: the GUI calls setParameter() on every control it renders.
        return true;
    }

    if (!validateValue(value, def)) {
        if (failedParameters) {
            failedParameters->append(name);
        }
        reportError(CameraError::Code::ValueOutOfRange,
                    QStringLiteral("Invalid value for parameter: %1").arg(name),
                    CameraError::Severity::Warning, QStringList{ name });
        return false;
    }

    m_pendingParameters.insert(name, value);
    return true;
}

bool Hk16011Driver::setParameters(const QVariantMap &parameters,
                                  QStringList *failedParameters)
{
    QMutexLocker locker(&m_mutex);

    QStringList localFailed;
    QStringList criticalFailure;

    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        const QString &name = it.key();
        const ParameterDefinition def = m_parameterDefinitions.value(name);

        if (!m_parameterDefinitions.contains(name)) {
            localFailed.append(name);
            if (hk16011CriticalParameters().contains(name)) {
                criticalFailure.append(name);
            }
            continue;
        }
        if (def.isReadOnly) {
            continue;
        }
        if (!validateValue(it.value(), def)) {
            localFailed.append(name);
            if (hk16011CriticalParameters().contains(name)) {
                criticalFailure.append(name);
            }
        }
    }

    if (!criticalFailure.isEmpty()) {
        // Reject the whole batch: leaving half a geometry change applied would
        // desynchronise the SDK's reader from the device.
        QStringList allFailed;
        for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
            if (!m_parameterDefinitions.value(it.key()).isReadOnly
                && !allFailed.contains(it.key())) {
                allFailed.append(it.key());
            }
        }
        const QString description =
            QStringLiteral("Failed to set parameters: critical parameter(s) %1 failed; "
                           "the entire batch was rejected to keep the frame geometry "
                           "consistent. Failed: %2")
                .arg(criticalFailure.join(QStringLiteral(", ")),
                     allFailed.join(QStringLiteral(", ")));
        if (failedParameters) {
            failedParameters->append(allFailed);
        }
        reportError(CameraError::Code::InvalidParameter, description,
                    CameraError::Severity::Warning, allFailed);
        return false;
    }

    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        if (!localFailed.contains(it.key())) {
            m_pendingParameters.insert(it.key(), it.value());
        }
    }

    if (!localFailed.isEmpty()) {
        if (failedParameters) {
            failedParameters->append(localFailed);
        }
        reportError(CameraError::Code::InvalidParameter,
                    QStringLiteral("Failed to set parameters: %1")
                        .arg(localFailed.join(QStringLiteral(", "))),
                    CameraError::Severity::Warning, localFailed);
    }

    return localFailed.isEmpty();
}

bool Hk16011Driver::validateParameters()
{
    QMutexLocker locker(&m_mutex);

    for (auto it = m_pendingParameters.constBegin(); it != m_pendingParameters.constEnd();
         ++it) {
        if (!m_parameterDefinitions.contains(it.key())) {
            return false;
        }
        if (!validateValue(it.value(), m_parameterDefinitions.value(it.key()))) {
            return false;
        }
    }
    return true;
}

bool Hk16011Driver::flushPendingParameters(QStringList *failedParameters)
{
    QStringList localFailed;
    bool ok = true;

    for (auto it = m_pendingParameters.constBegin(); it != m_pendingParameters.constEnd();
         ++it) {
        if (!writeValue(it.key(), it.value())) {
            localFailed.append(it.key());
            ok = false;
        } else {
            m_parameters.insert(it.key(), it.value());
        }
    }
    m_pendingParameters.clear();

    if (!ok && failedParameters) {
        failedParameters->append(localFailed);
    }
    return ok;
}

bool Hk16011Driver::commitParameters(QStringList *failedParameters)
{
    QMutexLocker locker(&m_mutex);

    if (m_pendingParameters.isEmpty()) {
        return true;
    }

    QStringList localFailed;
    const bool ok = flushPendingParameters(&localFailed);

    if (!ok) {
        const QString description =
            QStringLiteral("The camera rejected: %1").arg(localFailed.join(QStringLiteral(", ")));
        if (failedParameters) {
            failedParameters->append(localFailed);
        }
        reportError(CameraError::Code::CommitFailed, description,
                    CameraError::Severity::Warning, localFailed);
    } else {
        DRIVER_DEBUG << "committed" << m_parameters.size() << "parameters";
    }

    return ok;
}

//==============================================================================
// Capture
//==============================================================================

bool Hk16011Driver::startCapture(int captureCount)
{
    QMutexLocker locker(&m_mutex);

    // Checked before the state test on purpose: while a capture is running the
    // state is Acquiring, not Connected, and a second startCapture() during
    // that window must be a no-op success rather than a spurious "not
    // connected" failure.
    if (m_capturing.load()) {
        return true;
    }

    if (m_state.load() != CameraState::Connected || !m_device) {
        reportError(CameraError::Code::NotConnected,
                    QStringLiteral("Cannot start capture: not connected to camera"));
        return false;
    }

    if (captureCount < 0) {
        reportError(CameraError::Code::InvalidParameter,
                    QStringLiteral("captureCount must not be negative"));
        return false;
    }

    // Staged geometry that was never committed would make the SDK's reader
    // disagree with the device about frame size, so apply it first.
    if (!m_pendingParameters.isEmpty()) {
        DRIVER_DEBUG << "flushing" << m_pendingParameters.size()
                     << "uncommitted parameters before capture";
        QStringList failed;
        flushPendingParameters(&failed);
    }

    // The SDK only learns the frame geometry from SetParamValue calls — it
    // keeps its own image_width/image_height, seeded only by writes, and
    // StartFetch rejects the request outright when they are still zero. Reading
    // the parameters is not enough, so push the current geometry back through
    // the wire. The values are unchanged, so this is idempotent.
    for (const QString &geometry : { QStringLiteral("image_width"),
                                     QStringLiteral("image_height") }) {
        const QVariant value = m_parameters.value(geometry);
        if (!value.isValid()) {
            reportError(CameraError::Code::StateInvalid,
                        QStringLiteral("Cannot start capture: %1 is unknown").arg(geometry));
            return false;
        }
        if (!writeValue(geometry, value)) {
            reportError(CameraError::Code::CommunicationError,
                        QStringLiteral("Cannot start capture: the camera rejected %1").arg(geometry));
            return false;
        }
    }

    m_captureCount = captureCount;
    m_framesDelivered.store(0);
    m_fetchStarted = false;
    m_capturing.store(true);
    m_state.store(CameraState::Acquiring);

    // Use the device's own acquisition modes: count 0 = live, 1 = single,
    // >= 2 = burst of count.
    const HK16011_ErrorCodeEnum capRc =
        HK16011_StartCapture(m_device, static_cast<quint32>(captureCount));
    if (capRc != HK16011_OK) {
        m_capturing.store(false);
        m_state.store(CameraState::Connected);
        reportSdkError(QStringLiteral("HK16011_StartCapture"), capRc);
        return false;
    }

    DRIVER_DEBUG << "capture started, count:" << captureCount;
    emit captureStarted(m_connectedCameraId);

    // A bounded `ACQ fetch <n>` is only accepted once all n frames are already
    // cached in the device's DDR3; asked for earlier the firmware answers
    // ERR 5 (device busy). Waiting for that takes as long as the exposure
    // does — seconds for a long burst — so it is polled from a timer rather
    // than blocking this thread. `ACQ fetch 0` has no such precondition and
    // starts straight away.
    if (captureCount == 0) {
        if (!startFetchLocked(0)) {
            return false;
        }
    } else {
        m_captureDeadline = QDateTime::currentMSecsSinceEpoch() + kCaptureTimeoutMs;
        m_captureTimer->start(kCapturePollMs);
    }

    return true;
}

bool Hk16011Driver::startFetchLocked(int fetchCount)
{
    const HK16011_ErrorCodeEnum fetchRc =
        HK16011_StartFetch(m_device, static_cast<quint32>(fetchCount));
    if (fetchRc != HK16011_OK) {
        reportSdkError(QStringLiteral("HK16011_StartFetch"), fetchRc);
        finishCaptureLocked();
        return false;
    }
    m_fetchStarted = true;
    return true;
}

void Hk16011Driver::pollCapture()
{
    QMutexLocker locker(&m_mutex);

    if (!m_capturing.load() || m_fetchStarted) {
        m_captureTimer->stop();
        return;
    }

    // Negative means the SDK returned an error code, not a frame count.
    const int ready = HK16011_GetFrameNumReady(m_device);
    if (ready < 0) {
        reportSdkError(QStringLiteral("HK16011_GetFrameNumReady"), ready);
        finishCaptureLocked();
        return;
    }

    if (ready >= m_captureCount) {
        if (!startFetchLocked(m_captureCount)) {
            return;
        }
        m_captureTimer->stop();
        return;
    }

    if (QDateTime::currentMSecsSinceEpoch() > m_captureDeadline) {
        reportError(CameraError::Code::Timeout,
                    QStringLiteral("The camera cached only %1 of the %2 requested frames "
                                   "within %3 s")
                        .arg(ready)
                        .arg(m_captureCount)
                        .arg(kCaptureTimeoutMs / 1000));
        finishCaptureLocked();
    }
}

void Hk16011Driver::finishCaptureLocked()
{
    if (!m_capturing.load()) {
        return;
    }
    m_capturing.store(false);
    if (m_captureTimer) {
        m_captureTimer->stop();
    }

    if (m_device) {
        HK16011_AbortFetch(m_device);
        HK16011_AbortCapture(m_device);
    }
    if (m_state.load() == CameraState::Acquiring) {
        m_state.store(CameraState::Connected);
    }

    DRIVER_DEBUG << "capture stopped after" << m_framesDelivered.load() << "frames";
    emit captureStopped(m_connectedCameraId);
}

void Hk16011Driver::stopCapture(int timeoutMs)
{
    // The SDK's aborts return as soon as the reader thread is torn down, so
    // there is nothing to wait for beyond that; the timeout is accepted for
    // interface compatibility.
    QMutexLocker locker(&m_mutex);
    Q_UNUSED(timeoutMs)
    finishCaptureLocked();
}

void Hk16011Driver::onSdkFrame(const HK16011_FrameStruct *frame, void *user)
{
    // Runs on the SDK's reader thread. No QObject, no signal, no allocation
    // beyond the image copy — just hand the frame to the Qt thread.
    auto *self = static_cast<Hk16011Driver *>(user);
    self->queueFrame(frame);
}

void Hk16011Driver::queueFrame(const HK16011_FrameStruct *frame)
{
    if (!frame || !frame->data || frame->width == 0 || frame->height == 0) {
        return;
    }

    const int width = static_cast<int>(frame->width);
    const int height = static_cast<int>(frame->height);
    const qsizetype byteCount = static_cast<qsizetype>(frame->width)
        * static_cast<qsizetype>(frame->height) * static_cast<qsizetype>(frame->bytes_per_pixel);

    QImage image(width, height, QImage::Format_Grayscale16);
    if (image.isNull()) {
        return;
    }
    std::memcpy(image.bits(), frame->data, static_cast<size_t>(byteCount));

    // The device sends little-endian uint16; QImage::Format_Grayscale16 is
    // host-endian. x86 and ARM are little-endian, but be explicit rather than
    // silently wrong on a big-endian build.
    if (Q_BYTE_ORDER == Q_BIG_ENDIAN && frame->bytes_per_pixel == 2) {
        for (int y = 0; y < height; ++y) {
            auto *line = reinterpret_cast<quint16 *>(image.scanLine(y));
            for (int x = 0; x < width; ++x) {
                const quint16 v = line[x];
                line[x] = static_cast<quint16>((v >> 8) | (v << 8));
            }
        }
    }

    QueuedFrame queued;
    queued.image = QSharedPointer<QImage>::create(image);
    // The ICameraDriver contract wants microseconds since epoch; the SDK's own
    // timestamp_us is CLOCK_MONOTONIC and cannot be used for that.
    queued.timestamp = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()) * 1000ULL;
    queued.frameNumber = static_cast<quint32>(m_frameNumber.fetch_add(1) + 1);

    bool wakeQtThread = false;
    {
        QMutexLocker locker(&m_frameQueueMutex);
        m_frameQueue.push_back(queued);
        // One wake-up per batch is enough; deliverQueuedFrames() drains the rest.
        if (!m_deliveryScheduled.exchange(true)) {
            wakeQtThread = true;
        }
    }

    if (wakeQtThread) {
        QMetaObject::invokeMethod(this, "deliverQueuedFrames", Qt::QueuedConnection);
    }
}

void Hk16011Driver::deliverQueuedFrames()
{
    std::deque<QueuedFrame> batch;
    {
        QMutexLocker locker(&m_frameQueueMutex);
        batch.swap(m_frameQueue);
        m_deliveryScheduled.store(false);
    }

    if (batch.empty()) {
        return;
    }

    const bool bounded = m_captureCount > 0;

    for (const QueuedFrame &queued : batch) {
        if (!m_capturing.load()) {
            break;
        }
        // Frames arrive in batches, so a batch can overrun a small capture
        // count. Emit exactly what the caller asked for and drop the rest —
        // ICameraDriver promises captureCount frames, not "at least".
        if (bounded && m_framesDelivered.load() >= m_captureCount) {
            break;
        }
        const int frameNumber = m_framesDelivered.fetch_add(1) + 1;
        QVariantMap parameters;
        {
            QMutexLocker locker(&m_mutex);
            parameters = m_parameters;
        }
        emit frameReady(queued.image, queued.timestamp, frameNumber, m_connectedCameraId,
                        parameters);
    }

    if (bounded && m_framesDelivered.load() >= m_captureCount) {
        QMutexLocker locker(&m_mutex);
        finishCaptureLocked();
    }
}
