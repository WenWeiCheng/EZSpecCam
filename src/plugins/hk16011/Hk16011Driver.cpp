#include "Hk16011Driver.h"
#include "gui/DebugMacros.h"

#include "hk16011.h"

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

/**
 * Per-parameter presentation metadata that EZSpecCam owns.
 *
 * The device reports no grouping and no help text of its own. Both would be a
 * second source of truth that drifts as soon as the firmware is touched, which
 * is why the SDK dropped its copies rather than let the two disagree. This table
 * is therefore the only place an hk16011 parameter is described and assigned to
 * a GUI group, and it doubles as the whitelist: a parameter the firmware offers
 * but this table does not list is not exposed at all, so a firmware-side
 * addition surfaces in the log instead of silently appearing in the GUI.
 *
 * Within a group the GUI sorts by ParameterDefinition::order, which the driver
 * leaves at the LISTPARAMS index, so each category keeps the firmware's own
 * ordering without anything being spelled out here.
 *
 * `unitScale` is cumulative from the base unit, not the step between neighbours:
 * ParameterConstraint::getUnitIndex() compares the raw value against these
 * entries while toDisplayValue() divides by them, and the two only agree under
 * the cumulative reading. {"us","ms","s"} therefore needs {1000, 1000000} —
 * {1000, 1000} would render 2000 us as "2 s".
 */
struct ParameterMetadata
{
    ParameterCategory category;
    QString description;          ///< shown as the GUI tooltip; empty falls back to the SDK's
    QStringList units{};          ///< empty: keep whatever unit the SDK reported
    QVector<double> unitScale{};  ///< empty: no unit selector
};

const QMap<QString, ParameterMetadata> &hk16011ParameterMetadata()
{
    static const QMap<QString, ParameterMetadata> table = {
        // —— Core ——
        { QStringLiteral("exposure_time_us"),
          { ParameterCategory::Core,
            QStringLiteral("Integration time per frame, in microseconds."),
            { QStringLiteral("us"), QStringLiteral("ms"), QStringLiteral("s") },
            { 1000.0, 1000000.0 } } },
        { QStringLiteral("read_mode"),
          { ParameterCategory::Core,
            QStringLiteral("Line binning combines the 64 sensor rows into a single 1024x1 "
                           "line; image reads the full 1024x64 frame.") } },
        { QStringLiteral("freq_sel"),
          { ParameterCategory::Core,
            QStringLiteral("Pixel clock used to shift accumulated charge out of the CCD. "
                           "It bounds the shortest exposure the sensor can use.") } },
        { QStringLiteral("adc_gain_r"),
          { ParameterCategory::Core,
            QStringLiteral("Gain code applied to the red channel by the on-chip ADC (0-63).") } },
        { QStringLiteral("adc_gain_g"),
          { ParameterCategory::Core,
            QStringLiteral("Gain code applied to the green channel by the on-chip ADC (0-63).") } },
        { QStringLiteral("adc_gain_b"),
          { ParameterCategory::Core,
            QStringLiteral("Gain code applied to the blue channel by the on-chip ADC (0-63).") } },
        { QStringLiteral("adc_offset_r"),
          { ParameterCategory::Core,
            QStringLiteral("Offset code subtracted on the red channel before conversion (0-511).") } },
        { QStringLiteral("adc_offset_g"),
          { ParameterCategory::Core,
            QStringLiteral("Offset code subtracted on the green channel before conversion (0-511).") } },
        { QStringLiteral("adc_offset_b"),
          { ParameterCategory::Core,
            QStringLiteral("Offset code subtracted on the blue channel before conversion (0-511).") } },

        // —— Cooling ——
        { QStringLiteral("tec_enable"),
          { ParameterCategory::Cooling,
            QStringLiteral("Turns the thermoelectric cooler on; it closes the loop around "
                           "tec_set_temp.") } },
        { QStringLiteral("tec_set_temp"),
          { ParameterCategory::Cooling,
            QStringLiteral("Temperature the cooler loop drives towards, in degrees Celsius.") } },
        { QStringLiteral("sensor_temp"),
          { ParameterCategory::Cooling,
            QStringLiteral("Temperature reported at the sensor package.") } },
        { QStringLiteral("environment_temp"),
          { ParameterCategory::Cooling,
            QStringLiteral("Ambient temperature reported near the camera.") } },
        { QStringLiteral("tec_voltage"),
          { ParameterCategory::Cooling,
            QStringLiteral("Voltage the cooler is being driven at.") } },
        { QStringLiteral("tec_current"),
          { ParameterCategory::Cooling,
            QStringLiteral("Current the cooler is drawing.") } },

        // —— Info ——
        { QStringLiteral("camera_name"),
          { ParameterCategory::Info,
            QStringLiteral("Model name the camera reports for itself.") } },

        // —— Advanced ——
        { QStringLiteral("tec_kp"),
          { ParameterCategory::Advanced,
            QStringLiteral("Proportional term of the cooler PID loop.") } },
        { QStringLiteral("tec_ki"),
          { ParameterCategory::Advanced,
            QStringLiteral("Integral term of the cooler PID loop.") } },
        { QStringLiteral("tec_kd"),
          { ParameterCategory::Advanced,
            QStringLiteral("Derivative term of the cooler PID loop.") } },

        // —— Debug ——
        { QStringLiteral("mock_mode"),
          { ParameterCategory::Debug,
            QStringLiteral("Replaces the sensor with a synthetic test pattern. For bench "
                           "bring-up, not for measurements.") } },
        { QStringLiteral("cdsclk_delay"),
          { ParameterCategory::Debug,
            QStringLiteral("Extra clock cycles inserted between correlated double sampling "
                           "samples, in CCD clocks.") } },
        { QStringLiteral("image_width"),
          { ParameterCategory::Debug,
            QStringLiteral("Number of valid pixels per line the camera programs.") } },
        { QStringLiteral("image_height"),
          { ParameterCategory::Debug,
            QStringLiteral("Number of sensor lines the camera programs.") } },
        { QStringLiteral("bevel_left"),
          { ParameterCategory::Debug,
            QStringLiteral("Pixels discarded at the left edge of each line.") } },
        { QStringLiteral("bevel_top"),
          { ParameterCategory::Debug,
            QStringLiteral("Pixels discarded along the top edge.") } },
        { QStringLiteral("bevel_right"),
          { ParameterCategory::Debug,
            QStringLiteral("Pixels discarded at the right edge of each line.") } },
        { QStringLiteral("bevel_bottom"),
          { ParameterCategory::Debug,
            QStringLiteral("Pixels discarded along the bottom edge.") } },
        { QStringLiteral("blank_left"),
          { ParameterCategory::Debug,
            QStringLiteral("Pixels left at zero along the left edge.") } },
        { QStringLiteral("blank_right"),
          { ParameterCategory::Debug,
            QStringLiteral("Pixels left at zero along the right edge.") } },
        { QStringLiteral("acq_state"),
          { ParameterCategory::Debug,
            QStringLiteral("What the acquisition engine is doing right now: idle, exposing "
                           "or reading.") } },
        { QStringLiteral("frame_num_ready"),
          { ParameterCategory::Debug,
            QStringLiteral("Frames already buffered in the camera that have not been fetched. "
                           "A bounded fetch is only accepted once this reaches the count.") } },
        { QStringLiteral("frame_capacity"),
          { ParameterCategory::Debug,
            QStringLiteral("How many frames the camera's buffer can hold.") } },
        { QStringLiteral("exception_flag"),
          { ParameterCategory::Debug,
            QStringLiteral("Set when the firmware has raised an exception; read exception_cnt "
                           "alongside it.") } },
        { QStringLiteral("exception_cnt"),
          { ParameterCategory::Debug,
            QStringLiteral("Exception code or count recorded by the firmware.") } },
    };
    return table;
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
    // The SDK has no separate discovery call, so enumerate() opens the device
    // and closes it again. That costs ~365 ms — almost all of it in the 35 UART
    // round trips HK16011_Open spends priming its parameter cache (LISTPARAMS
    // plus one GETINFO per parameter at 115200 baud) — and enumerate() runs on
    // every scan: at start-up and again on each "Scan Plugins" click, on the
    // thread that also forwards frameReady() from the driver.
    //
    // Worth knowing when this is slow: an open failure makes the camera simply
    // not appear in the list, and PluginLoader ignores the return value of
    // enumerate(), so nothing is reported. That is the trade-off taken here —
    // the SDK is the only thing that can confirm the UART bridge is present as
    // well as the USB device.
    HK16011_DeviceHandle *device = nullptr;
    if (HK16011_Open(&device) != HK16011_OK || device == nullptr) {
        return QStringList();
    }

    const char *id = HK16011_GetDeviceId(device);
    const QString cameraId = (id && id[0] != '\0') ? QString::fromLatin1(id)
                                                   : hk16011CameraId();
    HK16011_Close(device);

    return QStringList{ cameraId };
}

//==============================================================================
// Connection
//==============================================================================

bool Hk16011Driver::connectToCamera(const QString &cameraId)
{
    QMutexLocker locker(&m_mutex);

    // Accept the id exactly as enumerate() produced it. hk16011CameraId()
    // rebuilds the same string the SDK returns, but comparing case
    // insensitively keeps the two from ever disagreeing.
    if (cameraId.compare(hk16011CameraId(), Qt::CaseInsensitive) != 0) {
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
    m_enumLabelByToken.clear();
    m_enumTokenByLabel.clear();
}

void Hk16011Driver::applyParameterMetadata()
{
    const QMap<QString, ParameterMetadata> &table = hk16011ParameterMetadata();

    // Query before defining, in both directions: a firmware parameter nobody
    // here describes is not exposed, and a listed parameter the firmware has
    // dropped is simply skipped. Either way the table stays the single source of
    // truth without having to be edited in lockstep with the firmware.
    for (auto it = m_parameterDefinitions.begin(); it != m_parameterDefinitions.end();) {
        if (!table.contains(it.key())) {
            qInfo().noquote() << "hk16011: not exposing unlisted parameter" << it.key();
            m_parameters.remove(it.key());
            it = m_parameterDefinitions.erase(it);
            continue;
        }
        ++it;
    }

    for (auto it = table.constBegin(); it != table.constEnd(); ++it) {
        auto def = m_parameterDefinitions.find(it.key());
        if (def == m_parameterDefinitions.end()) {
            qInfo().noquote() << "hk16011: firmware no longer offers" << it.key()
                              << "- drop it from hk16011ParameterMetadata()";
            continue;
        }
        def->category = it.value().category;
        if (!it.value().description.isEmpty()) {
            def->description = it.value().description;
        } else {
            // ParameterDefinition::isValid() rejects an empty description, so a
            // table entry that forgot one would make the parameter disappear.
            qInfo().noquote() << "hk16011:" << it.key()
                              << "has no description in hk16011ParameterMetadata(); "
                                 "falling back to the SDK's";
        }
        if (!it.value().units.isEmpty()) {
            def->constraint.unit = it.value().units;
            def->constraint.unitRange = it.value().unitScale;
        }
    }
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
            // validValues is what the GUI puts in a combo box, so it holds the
            // label. The device still speaks tokens, hence the two maps that
            // translate in both directions — validValues is also the validation
            // set and the value stored for the parameter, so a bare token would
            // never match the combo's currentData(). Items without a label (the
            // readout clock, "100k"/"500k") display as the token itself.
            QHash<QString, QString> labelByToken;
            QHash<QString, QString> tokenByLabel;
            const size_t itemCount =
                qMin(src.valid_item_count, static_cast<size_t>(HK16011_PARAM_ITEMS_MAX));
            for (size_t j = 0; j < itemCount; ++j) {
                const char *token = src.valid_items[j].data.s.set;
                if (!token) {
                    continue;
                }
                const QString wireToken = QString::fromLatin1(token);
                const char *label = src.valid_items[j].data.s.label;
                const QString shown =
                    (label && label[0] != '\0') ? QString::fromLatin1(label) : wireToken;
                def.constraint.validValues.append(shown);
                labelByToken.insert(wireToken, shown);
                tokenByLabel.insert(shown, wireToken);
            }
            if (!labelByToken.isEmpty()) {
                m_enumLabelByToken.insert(name, labelByToken);
                m_enumTokenByLabel.insert(name, tokenByLabel);
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

    // From here on the table holds only parameters EZSpecCam exposes, so the
    // passes below describe exactly what the GUI will render.
    applyParameterMetadata();

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

QVariant Hk16011Driver::toShownValue(const ParameterDefinition &def, const QVariant &value) const
{
    if (def.type != ParameterType::StringCollection || !value.isValid()) {
        return value;
    }
    const QString shown = m_enumLabelByToken.value(def.name).value(value.toString());
    return shown.isEmpty() ? value : QVariant(shown);
}

bool Hk16011Driver::fromVariant(const ParameterDefinition &def, const QVariant &value,
                                QByteArray *storage, HK16011_ValueStruct *out) const
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
    case ParameterType::StringCollection: {
        // SETPARAM wants the bare wire token with no label attached. An
        // unrecognised string is passed through as-is so a value that never
        // went through toShownValue() still reaches the device as typed.
        const QString shown = value.toString();
        *storage = m_enumTokenByLabel.value(def.name).value(shown, shown).toLatin1();
        out->type = HK16011_ValueType_Enumeration;
        out->data.s.set = storage->constData();
        out->data.s.label = nullptr;
        return true;
    }
    case ParameterType::String:
        *storage = value.toString().toLatin1();
        out->type = HK16011_ValueType_String;
        out->data.s.set = storage->constData();
        out->data.s.label = nullptr;
        return true;
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

    const QVariant result = toShownValue(m_parameterDefinitions.value(name), toVariant(value));
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
    // Must outlive the SDK call: the struct points its String/Enumeration
    // payload at these bytes. A QByteArray local to fromVariant() would leave
    // the SDK formatting freed memory.
    QByteArray storage;
    HK16011_ValueStruct sdkValue;
    if (!fromVariant(def, value, &storage, &sdkValue)) {
        qWarning().noquote() << "hk16011: no wire encoding for" << name
                             << "of ParameterType" << static_cast<int>(def.type);
        return false;
    }
    const QByteArray utf8Name = name.toLatin1();
    const HK16011_ErrorCodeEnum rc =
        HK16011_SetParamValue(m_device, utf8Name.constData(), &sdkValue);
    if (rc != HK16011_OK) {
        // Without this the caller only learns which parameter failed, never why,
        // which makes a device-side rejection indistinguishable from a bad value.
        qWarning().noquote() << "hk16011: SETPARAM" << name << "=" << value.toString()
                             << "rejected:" << rc << HK16011_ErrorCodeToString(rc);
    }
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

    // A wire token is a legitimate spelling of an enumeration value — configs
    // saved before the labels became the display form, and CLI scripts written
    // against the wire names, both carry them. Normalize first so everything
    // downstream sees one spelling.
    const QVariant shown = toShownValue(def, value);

    if (!validateValue(shown, def)) {
        if (failedParameters) {
            failedParameters->append(name);
        }
        reportError(CameraError::Code::ValueOutOfRange,
                    QStringLiteral("Invalid value for parameter: %1").arg(name),
                    CameraError::Severity::Warning, QStringList{ name });
        return false;
    }

    m_pendingParameters.insert(name, shown);
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
        if (!validateValue(toShownValue(def, it.value()), def)) {
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
            m_pendingParameters.insert(it.key(),
                                       toShownValue(m_parameterDefinitions.value(it.key()), it.value()));
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
    const int staged = m_pendingParameters.size();
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
        DRIVER_DEBUG << "committed" << staged << "parameters";
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

    // Live streams, so fetch continuously right away. A bounded count cannot:
    // `ACQ fetch <n>` is only accepted once all n frames are cached, and issuing
    // a continuous fetch while the acquisition is still running drops exactly
    // the last frame of every burst (measured on the bench at n = 2/5/8). Waiting
    // for the burst to finish and then asking for it whole is the only path that
    // returns every frame. See AGENTS.md.
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
    return true;
}

void Hk16011Driver::pollCapture()
{
    QMutexLocker locker(&m_mutex);

    if (!m_capturing.load()) {
        m_captureTimer->stop();
        return;
    }

    if (m_captureCount > 0) {
        // Wait until the device reports the whole count cached, then ask for it
        // in one bounded fetch.
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
