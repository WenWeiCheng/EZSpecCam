#include "HamamatsuDriver.h"
#include "gui/DebugMacros.h"

#include <QDateTime>
#include <QDebug>
#include <cstring>
#include <qthread.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <qvariant.h>

Q_LOGGING_CATEGORY(parameterCategory, "Parameter")
Q_LOGGING_CATEGORY(cameraCategory, "Camera")
Q_LOGGING_CATEGORY(configCategory, "Config")
Q_LOGGING_CATEGORY(displayCategory, "Display")
Q_LOGGING_CATEGORY(captureCategory, "Capture")
Q_LOGGING_CATEGORY(driverCategory, "Driver")

//==============================================================================
// Static variables
//==============================================================================

std::atomic<bool> HamamatsuDriver::s_sdkInitialized{false};
int HamamatsuDriver::s_sdkRefCount = 0;
const char *const HamamatsuDriver::s_driverVersion = "1.0.0";

//==============================================================================
// DCAM Property Name → ICameraDriver Parameter Name Mapping
//==============================================================================

// This mapping is necessary because DCAM uses long descriptive names
// while ICameraDriver expects short, standardized parameter names.
// Values come from the C16091-10 camera property enumeration output.
// All numerical constraints are read dynamically from the SDK.

static const QMap<QString, QString> &s_dcamToParamMap()
{
    static const QMap<QString, QString> map = {
        // Info category
        {"DCAM_IDSTR_VENDOR",              "vendor"},
        {"DCAM_IDSTR_MODEL",               "model"},
        {"DCAM_IDSTR_CAMERAID",            "camera_id"},
        {"DCAM_IDSTR_BUS",                 "bus"},
        {"DCAM_IDSTR_CAMERAVERSION",       "camera_version"},
        {"DCAM_IDSTR_DRIVERVERSION",       "driver_version"},
        {"DCAM_IDSTR_MODULEVERSION",       "module_version"},
        {"DCAM_IDSTR_DCAMAPIVERSION",      "dcamapi_version"},
        {"COLORTYPE",                      "color_type"},
        {"BIT PER CHANNEL",                "bit_depth"},
        {"IMAGE DETECTOR PIXEL NUM HORZ",  "detector_pixels_horz"},
        {"IMAGE DETECTOR PIXEL NUM VERT",  "detector_pixels_vert"},
        {"IMAGE WIDTH",                    "image_width"},
        {"IMAGE HEIGHT",                   "image_height"},
        {"IMAGE PIXEL TYPE",               "image_pixel_type"},
        {"BUFFER PIXEL TYPE",              "buffer_pixel_type"},
        {"SYSTEM ALIVE",                   "system_alive"},

        // Core category
        {"EXPOSURE TIME",                  "exposure"},
        {"CONTRAST GAIN",                  "contrast_gain"},
        {"TRIGGER SOURCE",                 "trigger_source"},
        {"TRIGGER MODE",                   "trigger_mode"},
        {"TRIGGER ACTIVE",                 "trigger_active"},
        {"TRIGGER POLARITY",               "trigger_polarity"},
        {"TRIGGER CONNECTOR",              "trigger_connector"},

        // Cooling category
        {"SENSOR TEMPERATURE",             "sensor_temperature"},
        {"SENSOR COOLER",                  "sensor_cooler"},
        {"SENSOR TEMPERATURE TARGET",      "sensor_temperature_target"},
        {"SENSOR COOLER STATUS",           "sensor_cooler_status"},

        // Advanced category
        {"READOUT FREQUECY",              "readout_frequency"},
        {"SENSOR MODE",                    "sensor_mode"},
        {"SENSOR MODE LINE BUNDLE HEIGHT", "line_bundle_height"},
        {"CAPTURE MODE",                   "capture_mode"},
        {"BINNING",                        "binning"},
    };
    return map;
}

static QMap<QString, QString> buildReverseMap()
{
    QMap<QString, QString> rev;
    for (auto it = s_dcamToParamMap().constBegin(); it != s_dcamToParamMap().constEnd(); ++it) {
        rev.insert(it.value(), it.key());
    }
    return rev;
}

//==============================================================================
// Excluded Property Names (matched case-insensitively)
//==============================================================================

static const QStringList &s_excludedProperties()
{
    static const QStringList list = {
        "SUBARRAY MODE",
        "SUBARRAY HPOS",
        "SUBARRAY HSIZE",
        "SUBARRAY VPOS",
        "SUBARRAY VSIZE",
        "TIMING READOUT TIME",
        "TIMING CYCLIC TRIGGER PERIOD",
        "TIMING MIN TRIGGER BLANKING",
        "TIMING MIN TRIGGER INTERVAL",
        "RECORD FIXED BYTES PER FILE",
        "RECORD FIXED BYTES PER SESSION",
        "RECORD FIXED BYTES PER FRAME",
    };
    return list;
}

//==============================================================================
// DCAM IDSTR (string info properties)
//==============================================================================

struct DcamIdStringInfo {
    int32 id;
    const char *paramName;
};

static const DcamIdStringInfo s_dcamIdStrings[] = {
    {DCAM_IDSTR_VENDOR,         "vendor"},
    {DCAM_IDSTR_MODEL,          "model"},
    {DCAM_IDSTR_CAMERAID,       "camera_id"},
    {DCAM_IDSTR_BUS,            "bus"},
    {DCAM_IDSTR_CAMERAVERSION,  "camera_version"},
    {DCAM_IDSTR_DRIVERVERSION,  "driver_version"},
    {DCAM_IDSTR_MODULEVERSION,  "module_version"},
    {DCAM_IDSTR_DCAMAPIVERSION, "dcamapi_version"},
};

//==============================================================================
// Construction / Destruction
//==============================================================================

HamamatsuDriver::HamamatsuDriver(QObject *parent)
    : ICameraDriver(parent)
{
}

HamamatsuDriver::~HamamatsuDriver()
{
    disconnectCamera();
    releaseSdk();
}

//==============================================================================
// SDK Lifecycle
//==============================================================================

bool HamamatsuDriver::initSdk()
{
    if (s_sdkInitialized.load()) {
        s_sdkRefCount++;
        return true;
    }

    DCAMAPI_INIT apiinit;
    std::memset(&apiinit, 0, sizeof(apiinit));
    apiinit.size = sizeof(apiinit);

    DCAMERR err = dcamapi_init(&apiinit);
    if (dcamFailed(err)) {
        DRIVER_DEBUG << "dcamapi_init failed:" << err;
        return false;
    }

    s_sdkInitialized.store(true);
    s_sdkRefCount = 1;
    DRIVER_DEBUG << "DCAMSDK4 initialized, devices found:" << apiinit.iDeviceCount;
    return true;
}

void HamamatsuDriver::releaseSdk()
{
    if (!s_sdkInitialized.load()) {
        return;
    }

    s_sdkRefCount--;
    if (s_sdkRefCount <= 0) {
        dcamapi_uninit();
        s_sdkInitialized.store(false);
        s_sdkRefCount = 0;
        DRIVER_DEBUG << "DCAMSDK4 uninitialized";
    }
}

//==============================================================================
// Discovery & Connection
//==============================================================================

QStringList HamamatsuDriver::enumerate()
{
    if (!s_sdkInitialized.load()) {
        if (!initSdk()) {
            return QStringList();
        }
    }

    DCAMAPI_INIT apiinit;
    std::memset(&apiinit, 0, sizeof(apiinit));
    apiinit.size = sizeof(apiinit);

    DCAMERR err = dcamapi_init(&apiinit);
    if (dcamFailed(err)) {
        DRIVER_DEBUG << "dcamapi_init enumerate failed:" << err;
        return QStringList();
    }

    QStringList cameras;
    int32 nDevice = apiinit.iDeviceCount;
    for (int32 i = 0; i < nDevice; i++) {
        QString desc = buildCameraIdString(i);
        if (!desc.isEmpty()) {
            cameras.append(desc);
        }
    }

    dcamapi_uninit();
    return cameras;
}

QString HamamatsuDriver::buildCameraIdString(int deviceIndex) const
{
    // Open the device briefly to get string info
    DCAMDEV_OPEN devopen;
    std::memset(&devopen, 0, sizeof(devopen));
    devopen.size = sizeof(devopen);
    devopen.index = deviceIndex;

    DCAMERR err = dcamdev_open(&devopen);
    if (dcamFailed(err)) {
        return QString();
    }

    HDCAM hdcam = devopen.hdcam;

    char vendor[256] = {0};
    char model[256] = {0};
    char cameraId[256] = {0};
    char bus[256] = {0};

    auto getString = [&](int32 idStr, char *buf, int32 bufSize) {
        DCAMDEV_STRING param;
        std::memset(&param, 0, sizeof(param));
        param.size = sizeof(param);
        param.text = buf;
        param.textbytes = bufSize;
        param.iString = idStr;
        dcamdev_getstring(hdcam, &param);
    };

    getString(DCAM_IDSTR_VENDOR, vendor, sizeof(vendor));
    getString(DCAM_IDSTR_MODEL, model, sizeof(model));
    getString(DCAM_IDSTR_CAMERAID, cameraId, sizeof(cameraId));
    getString(DCAM_IDSTR_BUS, bus, sizeof(bus));

    dcamdev_close(hdcam);

    // Format: "MODEL (CAMERAID) on BUS"  or  "MODEL (CAMERAID)" if bus empty
    QString desc = QString::fromLatin1(model);
    if (std::strlen(cameraId) > 0) {
        desc += QString(" (%1)").arg(QString::fromLatin1(cameraId));
    }
    if (std::strlen(bus) > 0) {
        desc += QString(" on %1").arg(QString::fromLatin1(bus));
    }
    return desc;
}

int HamamatsuDriver::deviceIndexFromId(const QString &cameraId) const
{
    DCAMAPI_INIT apiinit;
    std::memset(&apiinit, 0, sizeof(apiinit));
    apiinit.size = sizeof(apiinit);

    DCAMERR err = dcamapi_init(&apiinit);
    if (dcamFailed(err)) {
        return -1;
    }

    int32 nDevice = apiinit.iDeviceCount;
    int foundIndex = -1;

    for (int32 i = 0; i < nDevice; i++) {
        QString desc = buildCameraIdString(i);
        if (desc == cameraId) {
            foundIndex = static_cast<int>(i);
            break;
        }
    }

    dcamapi_uninit();
    return foundIndex;
}

bool HamamatsuDriver::connectToCamera(const QString &cameraId)
{
    QMutexLocker locker(&m_mutex);

    if (m_state.load() != CameraState::Disconnected) {
        disconnectCamera();
    }

    if (!s_sdkInitialized.load() && !initSdk()) {
        return false;
    }

    // Find device index from camera ID string
    m_deviceIndex = deviceIndexFromId(cameraId);
    if (m_deviceIndex < 0) {
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::ConnectionFailed,
            QString("Camera not found: %1").arg(cameraId)));
        releaseSdk();
        return false;
    }

    // Open the device
    DCAMDEV_OPEN devopen;
    std::memset(&devopen, 0, sizeof(devopen));
    devopen.size = sizeof(devopen);
    devopen.index = m_deviceIndex;

    DCAMERR err = dcamdev_open(&devopen);
    if (dcamFailed(err)) {
        emit errorOccurred(dcamErrToError(err, "dcamdev_open()"));
        releaseSdk();
        return false;
    }

    m_hdcam = devopen.hdcam;

    // Create wait handle
    DCAMWAIT_OPEN waitopen;
    std::memset(&waitopen, 0, sizeof(waitopen));
    waitopen.size = sizeof(waitopen);
    waitopen.hdcam = m_hdcam;

    err = dcamwait_open(&waitopen);
    if (dcamFailed(err)) {
        emit errorOccurred(dcamErrToError(err, "dcamwait_open()"));
        dcamdev_close(m_hdcam);
        m_hdcam = nullptr;
        releaseSdk();
        return false;
    }

    m_hwait = waitopen.hwait;

    m_connectedCameraId = cameraId;
    m_state.store(CameraState::Connecting);

    // Enumerate properties and build parameter definitions
    enumerateProperties();

    m_state.store(CameraState::Connected);
    emit connectionChanged(true, cameraId);
    DRIVER_DEBUG << "Connected to:" << cameraId;
    return true;
}

void HamamatsuDriver::disconnectCamera()
{
    QMutexLocker locker(&m_mutex);

    stopCapture();

    if (m_hwait != nullptr) {
        dcamwait_abort(m_hwait);
        // Wait handle is closed implicitly by dcamdev_close or needs explicit close?
        // The SDK docs indicate dcamwait_open/close are paired. But the samples
        // don't show a dcamwait_close. Let's be safe - the handle is tied to hdcam.
        m_hwait = nullptr;
    }

    if (m_hdcam != nullptr) {
        dcamdev_close(m_hdcam);
        m_hdcam = nullptr;
    }

    m_connectedCameraId.clear();
    m_deviceIndex = -1;
    m_parameterDefinitions.clear();
    m_parameters.clear();
    m_pendingParameters.clear();
    m_propertyIds.clear();
    m_dcamPropNames.clear();
    m_state.store(CameraState::Disconnected);

    emit connectionChanged(false, QString());

    releaseSdk();
}

bool HamamatsuDriver::isConnected() const
{
    return m_hdcam != nullptr && m_state.load() != CameraState::Disconnected;
}

//==============================================================================
// Parameter Interface
//==============================================================================

QStringList HamamatsuDriver::parameterNames() const
{
    QMutexLocker locker(&m_mutex);
    return m_parameterDefinitions.keys();
}

ParameterDefinition HamamatsuDriver::parameter(const QString &name) const
{
    QMutexLocker locker(&m_mutex);
    if (m_parameterDefinitions.contains(name)) {
        return m_parameterDefinitions.value(name);
    }
    return ParameterDefinition();
}

QVariant HamamatsuDriver::parameterValue(const QString &name) const
{
    QMutexLocker locker(&m_mutex);

    if (!m_parameterDefinitions.contains(name)) {
        return QVariant();
    }

    const ParameterDefinition &def = m_parameterDefinitions.value(name);

    // For dynamic/extrinsic parameters, always re-query from hardware
    if (def.isDynamic || def.isExtrinsic) {
        if (!m_propertyIds.contains(name)) {
            return QVariant();
        }
        int32 iProp = m_propertyIds.value(name);
        double value = 0;
        if (!getDcamPropertyValue(iProp, value)) {
            return QVariant();
        }

        // Convert to appropriate type
        switch (def.type) {
        case ParameterType::StringCollection: {
            // For MODE properties, get the text label
            char text[64] = {0};
            DCAMPROP_VALUETEXT pvt;
            std::memset(&pvt, 0, sizeof(pvt));
            pvt.cbSize = sizeof(pvt);
            pvt.iProp = iProp;
            pvt.value = value;
            pvt.text = text;
            pvt.textbytes = sizeof(text);

            DCAMERR err = dcamprop_getvaluetext(m_hdcam, &pvt);
            if (!dcamFailed(err)) {
                return QString::fromLatin1(text);
            }
            return QVariant();
        }
        case ParameterType::FloatRange:
        case ParameterType::FloatCollection:
            return value;
        case ParameterType::IntRange:
        case ParameterType::IntCollection:
            return static_cast<int>(value);
        case ParameterType::String: {
            // For string-type info properties (VENDOR, MODEL, etc.), use dcamdev_getstring
            // These are stored separately and not re-queried per-property
            if (m_parameters.contains(name)) {
                return m_parameters.value(name);
            }
            return QVariant();
        }
        default:
            return value;
        }
    }

    // For normal parameters, check pending first, then cached
    if (m_pendingParameters.contains(name)) {
        return m_pendingParameters.value(name);
    }
    if (m_parameters.contains(name)) {
        return m_parameters.value(name);
    }
    return QVariant();
}

bool HamamatsuDriver::setParameter(const QString &name, const QVariant &value)
{
    QMutexLocker locker(&m_mutex);

    if (!m_parameterDefinitions.contains(name)) {
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::InvalidParameter,
            QString("Unknown parameter: %1").arg(name)));
        return false;
    }

    const ParameterDefinition &def = m_parameterDefinitions.value(name);

    if (def.isReadOnly) {
        return true;
    }

    if (!validate(value, def.constraint, def.type)) {
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::ValueOutOfRange,
            QString("Invalid value for parameter: %1").arg(name)));
        return false;
    }

    m_pendingParameters.insert(name, value);
    return true;
}

bool HamamatsuDriver::validateParameters()
{
    QMutexLocker locker(&m_mutex);

    for (auto it = m_pendingParameters.constBegin(); it != m_pendingParameters.constEnd(); ++it) {
        const QString &name = it.key();
        const QVariant &value = it.value();

        if (!m_parameterDefinitions.contains(name)) {
            emit errorOccurred(CameraError::makeError(
                CameraError::Code::CommitFailed,
                QString("Unknown parameter: %1").arg(name)));
            m_pendingParameters.clear();
            return false;
        }

        const ParameterDefinition &def = m_parameterDefinitions.value(name);
        if (!validate(value, def.constraint, def.type)) {
            emit errorOccurred(CameraError::makeError(
                CameraError::Code::CommitFailed,
                QString("Invalid value for parameter: %1").arg(name)));
            m_pendingParameters.clear();
            return false;
        }
    }

    return true;
}

bool HamamatsuDriver::commitParameters()
{
    QMutexLocker locker(&m_mutex);

    if (!m_hdcam) {
        return false;
    }

    // Validate first
    if (!validateParameters()) {
        return false;
    }

    // Apply each pending parameter to the camera
    for (auto it = m_pendingParameters.constBegin(); it != m_pendingParameters.constEnd(); ++it) {
        const QString &name = it.key();
        const QVariant &value = it.value();

        if (!m_propertyIds.contains(name)) {
            continue;
        }

        int32 iProp = m_propertyIds.value(name);
        const ParameterDefinition &def = m_parameterDefinitions.value(name);

        double dcamValue = 0;

        switch (def.type) {
        case ParameterType::FloatCollection:
        case ParameterType::FloatRange:
            dcamValue = value.toDouble();
            break;
        case ParameterType::IntRange:
        case ParameterType::IntCollection:
            dcamValue = static_cast<double>(value.toInt());
            break;
        case ParameterType::StringCollection: {
            // For StringCollection, we need to map the text back to a numeric value
            // Try each valid value and compare text
            char text[64] = {0};
            QString targetText = value.toString();
            bool found = false;

            for (const QVariant &validVal : def.constraint.validValues) {
                DCAMPROP_VALUETEXT pvt;
                std::memset(&pvt, 0, sizeof(pvt));
                pvt.cbSize = sizeof(pvt);
                pvt.iProp = iProp;
                pvt.value = validVal.toDouble();
                pvt.text = text;
                pvt.textbytes = sizeof(text);

                DCAMERR err = dcamprop_getvaluetext(m_hdcam, &pvt);
                if (!dcamFailed(err) && targetText == QString::fromLatin1(text)) {
                    dcamValue = validVal.toDouble();
                    found = true;
                    break;
                }
            }

            if (!found) {
                // Fallback: try direct numeric conversion
                dcamValue = value.toDouble();
            }
            break;
        }
        default:
            continue;
        }

        if (!setDcamPropertyValue(iProp, dcamValue)) {
            emit errorOccurred(CameraError::makeError(
                CameraError::Code::CommitFailed,
                QString("Failed to set parameter: %1").arg(name)));
            m_pendingParameters.clear();
            return false;
        }

            m_parameters.insert(name, value);
    }

    m_pendingParameters.clear();
    return true;
}

//==============================================================================
// Capture
//==============================================================================

bool HamamatsuDriver::startCapture(int captureCount)
{
    QMutexLocker locker(&m_mutex);

    if (!m_hdcam || !m_hwait) {
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::StateInvalid, "Camera not connected"));
        return false;
    }

    if (m_capturing.load()) {
        return true;  // Already capturing
    }

    // Read image dimensions dynamically
    double width = 0, height = 0;
    getDcamPropertyValue(DCAM_IDPROP_IMAGE_WIDTH, width);
    getDcamPropertyValue(DCAM_IDPROP_IMAGE_HEIGHT, height);
    m_imageWidth = static_cast<int>(width);
    m_imageHeight = static_cast<int>(height);

    // Allocate DCAM internal buffers (10-30 frames typical)
    int32 frameCount = (captureCount > 0) ? qMin(captureCount, 30) : 30;
    DCAMERR err = dcambuf_alloc(m_hdcam, frameCount);
    if (dcamFailed(err)) {
        emit errorOccurred(dcamErrToError(err, "dcambuf_alloc()"));
        return false;
    }

    // Start capture in SEQUENCE mode
    err = dcamcap_start(m_hdcam, DCAMCAP_START_SEQUENCE);
    if (dcamFailed(err)) {
        dcambuf_release(m_hdcam);
        emit errorOccurred(dcamErrToError(err, "dcamcap_start()"));
        return false;
    }

    m_resourcesHeld.store(true);
    m_capturing.store(true);
    m_captureCountTarget.store(captureCount);
    m_framesCaptured.store(0);
    m_state.store(CameraState::Acquiring);

    // Start worker thread
    m_captureThread = QThread::create([this]() {
        onCaptureLoop();
    });
    m_captureThread->start();

    emit captureStarted(m_connectedCameraId);
    return true;
}

void HamamatsuDriver::stopCapture(int timeoutMs)
{
    QMutexLocker locker(&m_mutex);

    if (m_capturing.load()) {
        m_capturing.store(false);
        if (m_hwait) {
            dcamwait_abort(m_hwait);
        }
    }

    if (m_captureThread) {
        if (!m_captureThread->wait(timeoutMs)) {
            DRIVER_DEBUG << "Capture thread did not finish within timeout";
            m_captureThread->terminate();
            m_captureThread->wait(3000);
        }
        delete m_captureThread;
        m_captureThread = nullptr;
    }

    bool held = m_resourcesHeld.load();
    if (held) {
        m_resourcesHeld.store(false);
        if (m_hdcam) {
            dcamcap_stop(m_hdcam);
            dcambuf_release(m_hdcam);
        }
    }

    if (held) {
        m_state.store(CameraState::Connected);
        emit captureStopped(m_connectedCameraId);
    }
}

void HamamatsuDriver::onCaptureLoop()
{
    DCAMWAIT_START waitstart;
    std::memset(&waitstart, 0, sizeof(waitstart));
    waitstart.size = sizeof(waitstart);
    waitstart.eventmask = DCAMWAIT_CAPEVENT_FRAMEREADY;
    waitstart.timeout = 1000;

    DCAMBUF_FRAME bufframe;
    std::memset(&bufframe, 0, sizeof(bufframe));
    bufframe.size = sizeof(bufframe);
    bufframe.iFrame = -1;

    while (m_capturing.load()) {
        DCAMERR err = dcamwait_start(m_hwait, &waitstart);
        if (dcamFailed(err)) {
            if (err == DCAMERR_ABORT) {
                break;
            }
            if (err == DCAMERR_TIMEOUT) {
                continue;
            }
            DRIVER_DEBUG << "dcamwait_start error:" << err;
            continue;
        }

        err = dcambuf_lockframe(m_hdcam, &bufframe);
        if (dcamFailed(err)) {
            DRIVER_DEBUG << "dcambuf_lockframe error:" << err;
            continue;
        }

        int captured = m_framesCaptured.fetch_add(1) + 1;
        int target = m_captureCountTarget.load();
        if (target > 0 && captured >= target) {
            m_capturing.store(false);
        }

        QSharedPointer<QImage> image = convertFrameToImage(bufframe);

        quint64 timestamp = static_cast<quint64>(bufframe.timestamp.sec) * 1000000ULL
                          + static_cast<quint64>(bufframe.timestamp.microsec);

        QVariantMap params;
        for (auto it = m_parameters.constBegin(); it != m_parameters.constEnd(); ++it) {
            params.insert(it.key(), it.value());
        }

        emit frameReady(image, timestamp, bufframe.framestamp,
                        m_connectedCameraId, params);
    }

    QMetaObject::invokeMethod(this, "onCaptureCompleted", Qt::QueuedConnection);
}

void HamamatsuDriver::onCaptureCompleted()
{
    stopCapture();
}

QSharedPointer<QImage> HamamatsuDriver::convertFrameToImage(
    const DCAMBUF_FRAME &frame) const
{
    int width = frame.width;
    int height = frame.height;
    int rowbytes = frame.rowbytes;

    // Determine QImage format based on pixel type
    // For C16091-10, this is always MONO16 (B/W, 16-bit)
    // We read BUFFER PIXEL TYPE dynamically but default to MONO16
    QImage::Format format = QImage::Format_Grayscale16;

    QImage image(width, height, format);

    // Copy row by row, respecting rowbytes (may include padding)
    const char *src = static_cast<const char *>(frame.buf);
    int bytesPerPixel = 2;  // MONO16

    for (int y = 0; y < height; y++) {
        std::memcpy(image.scanLine(y), src + y * rowbytes,
                    static_cast<size_t>(width) * bytesPerPixel);
    }

    return QSharedPointer<QImage>::create(image);
}

//==============================================================================
// State / Info
//==============================================================================

CameraState HamamatsuDriver::state() const
{
    return m_state.load();
}

QString HamamatsuDriver::driverVersion() const
{
    return QString::fromLatin1(s_driverVersion);
}

QString HamamatsuDriver::cameraId() const
{
    return m_connectedCameraId;
}

//==============================================================================
// Property Enumeration
//==============================================================================

void HamamatsuDriver::enumerateProperties()
{
    m_parameterDefinitions.clear();
    m_parameters.clear();
    m_pendingParameters.clear();
    m_propertyIds.clear();
    m_dcamPropNames.clear();

    if (!m_hdcam) {
        return;
    }

    // Read string-type info properties first (VENDOR, MODEL, etc.)
    for (const auto &info : s_dcamIdStrings) {
        char text[256] = {0};
        DCAMDEV_STRING param;
        std::memset(&param, 0, sizeof(param));
        param.size = sizeof(param);
        param.text = text;
        param.textbytes = sizeof(text);
        param.iString = info.id;

        DCAMERR err = dcamdev_getstring(m_hdcam, &param);
        if (!dcamFailed(err)) {
            ParameterDefinition def;
            def.name = QString::fromLatin1(info.paramName);
            def.displayName = def.name;
            def.category = ParameterCategory::Info;
            def.type = ParameterType::String;
            def.isReadOnly = true;
            def.constraint = ParameterConstraint();
            m_parameterDefinitions.insert(def.name, def);
            m_parameters.insert(def.name, QString::fromLatin1(text));
        }
    }

    // Enumerate all supported DCAM properties
    int32 iProp = 0;
    DCAMERR err = dcamprop_getnextid(m_hdcam, &iProp, DCAMPROP_OPTION_SUPPORT);
    if (dcamFailed(err)) {
        DRIVER_DEBUG << "dcamprop_getnextid failed:" << err;
        return;
    }

    do {
        // Get property name
        char nameBuf[128] = {0};
        err = dcamprop_getname(m_hdcam, iProp, nameBuf, sizeof(nameBuf));
        if (dcamFailed(err)) {
            err = dcamprop_getnextid(m_hdcam, &iProp, DCAMPROP_OPTION_SUPPORT);
            if (dcamFailed(err) || iProp == 0) break;
            continue;
        }

        QString propName = QString::fromLatin1(nameBuf);

        // Check exclusion list
        if (isExcluded(propName)) {
            err = dcamprop_getnextid(m_hdcam, &iProp, DCAMPROP_OPTION_SUPPORT);
            if (dcamFailed(err) || iProp == 0) break;
            continue;
        }

        // Get property attributes
        DCAMPROP_ATTR attr;
        std::memset(&attr, 0, sizeof(attr));
        attr.cbSize = sizeof(attr);
        attr.iProp = iProp;

        err = dcamprop_getattr(m_hdcam, &attr);
        if (dcamFailed(err)) {
            err = dcamprop_getnextid(m_hdcam, &iProp, DCAMPROP_OPTION_SUPPORT);
            if (dcamFailed(err) || iProp == 0) break;
            continue;
        }

        // Store DCAM property name
        m_dcamPropNames.insert(iProp, propName);

        // Build parameter definition
        ParameterDefinition def = buildParameterDefinition(iProp, propName, attr);
        if (def.isValid()) {
            m_parameterDefinitions.insert(def.name, def);
            m_propertyIds.insert(def.name, iProp);

            // Read current value
            double currentValue = 0;
            if (getDcamPropertyValue(iProp, currentValue)) {
                // Convert to appropriate type for storage
                switch (def.type) {
                case ParameterType::FloatRange:
                case ParameterType::FloatCollection:
                    m_parameters.insert(def.name, currentValue);
                    break;
                case ParameterType::IntRange:
                case ParameterType::IntCollection:
                    m_parameters.insert(def.name, static_cast<int>(currentValue));
                    break;
                case ParameterType::StringCollection:
                case ParameterType::String: {
                    // Try to get text label
                    char text[64] = {0};
                    DCAMPROP_VALUETEXT pvt;
                    std::memset(&pvt, 0, sizeof(pvt));
                    pvt.cbSize = sizeof(pvt);
                    pvt.iProp = iProp;
                    pvt.value = currentValue;
                    pvt.text = text;
                    pvt.textbytes = sizeof(text);

                    DCAMERR vtErr = dcamprop_getvaluetext(m_hdcam, &pvt);
                    if (!dcamFailed(vtErr)) {
                        m_parameters.insert(def.name, QString::fromLatin1(text));
                    } else {
                        m_parameters.insert(def.name, currentValue);
                    }
                    break;
                }
                default:
                    m_parameters.insert(def.name, currentValue);
                    break;
                }
            }
        }

        // Next property
        err = dcamprop_getnextid(m_hdcam, &iProp, DCAMPROP_OPTION_SUPPORT);
        if (dcamFailed(err) || iProp == 0) break;

    } while (iProp != 0);
}

ParameterDefinition HamamatsuDriver::buildParameterDefinition(
    int32 iProp, const QString &propName, const DCAMPROP_ATTR &attr) const
{
    ParameterDefinition def;

    // Map DCAM property name to ICameraDriver parameter name
    QString paramName = mapPropertyName(propName);
    if (paramName.isEmpty()) {
        // Unknown property - skip
        return def;
    }

    def.name = paramName;
    def.displayName = paramName;
    def.category = categorizeProperty(propName, iProp);

    bool isWritable = (attr.attribute & DCAMPROP_ATTR_WRITABLE) != 0;
    def.isReadOnly = !isWritable;

    // Check for VOLATILE attribute (dynamic value)
    bool isVolatile = (attr.attribute & DCAMPROP_ATTR_VOLATILE) != 0;

    // Determine DCAM property type
    int32 dcamType = attr.attribute & DCAMPROP_TYPE_MASK;

    // Check for valuetext support
    bool hasValueText = (attr.attribute & DCAMPROP_ATTR_HASVALUETEXT) != 0;

    if (def.category == ParameterCategory::Info) {
        if (dcamType == DCAMPROP_TYPE_MODE && hasValueText) {
            def.type = ParameterType::StringCollection;
            def.isReadOnly = true;

            // Enumerate mode values
            if (hasValueText) {
                double v = attr.valuemin;
                do {
                    char text[64] = {0};
                    DCAMPROP_VALUETEXT pvt;
                    std::memset(&pvt, 0, sizeof(pvt));
                    pvt.cbSize = sizeof(pvt);
                    pvt.iProp = iProp;
                    pvt.value = v;
                    pvt.text = text;
                    pvt.textbytes = sizeof(text);

                    DCAMERR err = dcamprop_getvaluetext(m_hdcam, &pvt);
                    if (!dcamFailed(err)) {
                        def.constraint.validValues.append(QString::fromLatin1(text));
                    }

                    // Get next value
                    err = dcamprop_queryvalue(m_hdcam, iProp, &v, DCAMPROP_OPTION_NEXT);
                    if (dcamFailed(err)) break;
                } while (true);
            }
        } else if (dcamType == DCAMPROP_TYPE_LONG) {
            def.type = ParameterType::IntRange;
            def.isReadOnly = true;
        } else if (dcamType == DCAMPROP_TYPE_REAL) {
            def.type = ParameterType::FloatRange;
            def.isReadOnly = true;
        }

        return def;
    }

    // For non-Info properties, map based on DCAM type
    switch (dcamType) {
    case DCAMPROP_TYPE_MODE:
        // MODE type always has text labels
        def.type = ParameterType::StringCollection;
        if (hasValueText) {
            double v = attr.valuemin;
            do {
                char text[64] = {0};
                DCAMPROP_VALUETEXT pvt;
                std::memset(&pvt, 0, sizeof(pvt));
                pvt.cbSize = sizeof(pvt);
                pvt.iProp = iProp;
                pvt.value = v;
                pvt.text = text;
                pvt.textbytes = sizeof(text);

                DCAMERR err = dcamprop_getvaluetext(m_hdcam, &pvt);
                if (!dcamFailed(err)) {
                    def.constraint.validValues.append(QString::fromLatin1(text));
                }

                err = dcamprop_queryvalue(m_hdcam, iProp, &v, DCAMPROP_OPTION_NEXT);
                if (dcamFailed(err)) break;
            } while (true);
        }
        break;

    case DCAMPROP_TYPE_LONG: {
        bool hasRange = (attr.attribute & DCAMPROP_ATTR_HASRANGE) != 0;
        bool hasStep = (attr.attribute & DCAMPROP_ATTR_HASSTEP) != 0;

        if (hasValueText) {
            // LONG with valuetext → IntCollection
            def.type = ParameterType::IntCollection;
            // Enumerate values via valuetext
            double v = attr.valuemin;
            do {
                char text[64] = {0};
                DCAMPROP_VALUETEXT pvt;
                std::memset(&pvt, 0, sizeof(pvt));
                pvt.cbSize = sizeof(pvt);
                pvt.iProp = iProp;
                pvt.value = v;
                pvt.text = text;
                pvt.textbytes = sizeof(text);

                DCAMERR err = dcamprop_getvaluetext(m_hdcam, &pvt);
                if (!dcamFailed(err)) {
                    def.constraint.validValues.append(static_cast<int>(v));
                }

                err = dcamprop_queryvalue(m_hdcam, iProp, &v, DCAMPROP_OPTION_NEXT);
                if (dcamFailed(err)) break;
            } while (true);
        } else {
            // LONG without valuetext → IntRange
            def.type = ParameterType::IntRange;
            if (hasRange) {
                def.constraint.minValue = attr.valuemin;
                def.constraint.maxValue = attr.valuemax;
            }
            if (hasStep) {
                def.constraint.step = attr.valuestep;
            }
        }
        break;
    }

    case DCAMPROP_TYPE_REAL: {
        bool hasRange = (attr.attribute & DCAMPROP_ATTR_HASRANGE) != 0;
        bool hasStep = (attr.attribute & DCAMPROP_ATTR_HASSTEP) != 0;

        // Check if this is READOUT FREQUENCY → use FloatCollection
        if (paramName == "readout_frequency") {
            def.type = ParameterType::FloatCollection;
            // Generate discrete values from min/max/step
            if (hasRange && hasStep && attr.valuestep > 0) {
                for (double v = attr.valuemin; v <= attr.valuemax + (attr.valuestep * 0.5); v += attr.valuestep) {
                    def.constraint.validValues.append(v);
                }
            }
        } else {
            def.type = ParameterType::FloatRange;
            if (hasRange) {
                def.constraint.minValue = attr.valuemin;
                def.constraint.maxValue = attr.valuemax;
            }
            if (hasStep) {
                def.constraint.step = attr.valuestep;
            }
        }
        break;
    }

    default:
        // Unknown type - skip
        return def;
    }

    // Set dynamic/extrinsic flags for volatile properties
    if (isVolatile) {
        if (paramName == "system_alive") {
            def.isDynamic = true;
            def.isExtrinsic = true;
        } else if (paramName.startsWith("sensor_temperature") || paramName == "sensor_cooler_status") {
            def.isDynamic = true;
            def.isExtrinsic = true;
        }
    }

    // Set default value
    if (attr.attribute & DCAMPROP_ATTR_HASDEFAULT) {
        switch (def.type) {
        case ParameterType::FloatRange:
        case ParameterType::FloatCollection:
            def.defaultValue = attr.valuedefault;
            break;
        case ParameterType::IntRange:
        case ParameterType::IntCollection:
            def.defaultValue = static_cast<int>(attr.valuedefault);
            break;
        case ParameterType::StringCollection: {
            // Get text for default value
            char text[64] = {0};
            DCAMPROP_VALUETEXT pvt;
            std::memset(&pvt, 0, sizeof(pvt));
            pvt.cbSize = sizeof(pvt);
            pvt.iProp = iProp;
            pvt.value = attr.valuedefault;
            pvt.text = text;
            pvt.textbytes = sizeof(text);

            DCAMERR err = dcamprop_getvaluetext(m_hdcam, &pvt);
            if (!dcamFailed(err)) {
                def.defaultValue = QString::fromLatin1(text);
            } else {
                def.defaultValue = attr.valuedefault;
            }
            break;
        }
        default:
            def.defaultValue = attr.valuedefault;
            break;
        }
    }

    return def;
}

ParameterCategory HamamatsuDriver::categorizeProperty(
    const QString &propName, int32 iProp) const
{
    Q_UNUSED(iProp)

    // String info properties
    if (propName.startsWith("DCAM_IDSTR_")) {
        return ParameterCategory::Info;
    }

    // Detect by parameter name (after mapping)
    QString paramName = mapPropertyName(propName);

    // Info category
    static const QStringList infoParams = {
        "vendor", "model", "camera_id", "bus",
        "camera_version", "driver_version", "module_version", "dcamapi_version",
        "color_type", "bit_depth",
        "detector_pixels_horz", "detector_pixels_vert",
        "image_width", "image_height",
        "image_pixel_type", "buffer_pixel_type",
        "system_alive"
    };

    if (infoParams.contains(paramName)) {
        return ParameterCategory::Info;
    }

    // Cooling category
    if (paramName.startsWith("sensor_")) {
        return ParameterCategory::Cooling;
    }

    // Core category
    static const QStringList coreParams = {
        "exposure", "contrast_gain",
        "trigger_source", "trigger_mode", "trigger_active",
        "trigger_polarity", "trigger_connector", "binning"
    };

    if (coreParams.contains(paramName)) {
        return ParameterCategory::Core;
    }

    // Advanced category (everything else)
    return ParameterCategory::Advanced;
}

QString HamamatsuDriver::mapPropertyName(const QString &dcamName) const
{
    const auto &map = s_dcamToParamMap();

    // Direct lookup
    if (map.contains(dcamName)) {
        return map.value(dcamName);
    }

    // Case-insensitive fallback
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        if (it.key().compare(dcamName, Qt::CaseInsensitive) == 0) {
            return it.value();
        }
    }

    return QString();
}

QString HamamatsuDriver::reverseMapPropertyName(const QString &paramName) const
{
    static const QMap<QString, QString> reverseMap = buildReverseMap();
    return reverseMap.value(paramName);
}

bool HamamatsuDriver::isExcluded(const QString &propName) const
{
    for (const QString &excluded : s_excludedProperties()) {
        if (propName.compare(excluded, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

//==============================================================================
// DCAM Property I/O
//==============================================================================

bool HamamatsuDriver::getDcamPropertyValue(int32 iProp, double &value) const
{
    if (!m_hdcam) {
        return false;
    }

    value = 0;
    DCAMERR err = dcamprop_getvalue(m_hdcam, iProp, &value);
    return !dcamFailed(err);
}

bool HamamatsuDriver::setDcamPropertyValue(int32 iProp, double value)
{
    if (!m_hdcam) {
        return false;
    }

    DCAMERR err = dcamprop_setvalue(m_hdcam, iProp, value);
    return !dcamFailed(err);
}

CameraError HamamatsuDriver::dcamErrToError(
    DCAMERR err, const QString &context) const
{
    Q_UNUSED(context)

    switch (err) {
    case DCAMERR_INVALIDPARAM:
        return CameraError::makeError(
            CameraError::Code::InvalidParameter, context);
    case DCAMERR_INVALIDHANDLE:
        return CameraError::makeError(
            CameraError::Code::NotConnected, context);
    case DCAMERR_TIMEOUT:
        return CameraError::makeError(
            CameraError::Code::Timeout, context);
    case DCAMERR_NOTSUPPORT:
        return CameraError::makeError(
            CameraError::Code::NotSupported, context);
    case DCAMERR_NOCAMERA:
    case DCAMERR_NODRIVER:
        return CameraError::makeError(
            CameraError::Code::ConnectionFailed, context);
    case DCAMERR_BUSY:
        return CameraError::makeError(
            CameraError::Code::StateInvalid, context);
    default:
        return CameraError::makeError(
            CameraError::Code::DriverError,
            QString("%1 (DCAMERR: 0x%2)").arg(context).arg(static_cast<int>(err), 0, 16));
    }
}
