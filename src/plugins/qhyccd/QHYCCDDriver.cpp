#include "QHYCCDDriver.h"

#include <QDateTime>
#include <QDebug>
#include <cstring>

// Static variables
std::atomic<bool> QHYCCDDriver::s_sdkInitialized{false};
int QHYCCDDriver::s_sdkRefCount = 0;

QHYCCDDriver::QHYCCDDriver(QObject *parent)
    : ICameraDriver(parent)
    , m_cameraHandle(nullptr)
    , m_captureThread(nullptr)
    , m_state(CameraState::Disconnected)
    , m_imageWidth(0)
    , m_imageHeight(0)
    , m_imageBytes(0)
    , m_bufferSize(0)
{
}

QHYCCDDriver::~QHYCCDDriver()
{
    disconnectCamera();
    releaseSdk();
}

QStringList QHYCCDDriver::enumerate()
{
    if (!s_sdkInitialized.load()) {
        if (!initSdk()) {
            return QStringList();
        }
    }

    uint32_t numCameras = ScanQHYCCD();
    QStringList cameras;

    char id[64] = {0};
    for (uint32_t i = 0; i < numCameras; i++) {
        memset(id, 0, 64);
        uint32_t ret = GetQHYCCDId(i, id);
        if (ret == QHYCCD_SUCCESS) {
            cameras.append(QString::fromLatin1(id));
        }
    }

    return cameras;
}

bool QHYCCDDriver::connectToCamera(const QString &cameraId)
{
    QMutexLocker locker(&m_mutex);

    if (m_connected.load()) {
        disconnectCamera();
    }

    if (!s_sdkInitialized.load() && !initSdk()) {
        return false;
    }

    m_cameraHandle = OpenQHYCCD(cameraId.toLatin1().data());
    if (m_cameraHandle == nullptr) {
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::ConnectionFailed,
            QString("Failed to open camera: %1").arg(cameraId)));
        return false;
    }

    m_connectedCameraId = cameraId;
    m_state.store(CameraState::Connected);

    // Initialize parameters from camera
    initializeParameterDefinitions();

    emit connectionChanged(true, cameraId);
    return true;
}

void QHYCCDDriver::disconnectCamera()
{
    QMutexLocker locker(&m_mutex);

    stopCapture();

    if (m_cameraHandle != nullptr) {
        CloseQHYCCD(m_cameraHandle);
        m_cameraHandle = nullptr;
    }

    m_connected.store(false);
    m_state.store(CameraState::Disconnected);
    m_connectedCameraId.clear();

    emit connectionChanged(false, QString());
}

bool QHYCCDDriver::isConnected() const
{
    return m_connected.load();
}

QStringList QHYCCDDriver::parameterNames() const
{
    QMutexLocker locker(&m_mutex);
    return m_parameterDefinitions.keys();
}

ParameterDefinition QHYCCDDriver::parameter(const QString &name) const
{
    QMutexLocker locker(&m_mutex);
    if (m_parameterDefinitions.contains(name)) {
        return m_parameterDefinitions.value(name);
    }
    return ParameterDefinition();
}

QVariant QHYCCDDriver::parameterValue(const QString &name) const
{
    QMutexLocker locker(&m_mutex);
    if (m_pendingParameters.contains(name)) {
        return m_pendingParameters.value(name);
    }
    return m_parameters.value(name);
}

bool QHYCCDDriver::setParameter(const QString &name, const QVariant &value)
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

    if (!validateValue(value, def)) {
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::ValueOutOfRange,
            QString("Invalid value for parameter: %1").arg(name)));
        return false;
    }

    m_pendingParameters.insert(name, value);
    return true;
}

bool QHYCCDDriver::validateParameters()
{
    QMutexLocker locker(&m_mutex);

    for (auto it = m_pendingParameters.constBegin(); it != m_pendingParameters.constEnd(); ++it) {
        const QString &name = it.key();
        const QVariant &value = it.value();

        if (!m_parameterDefinitions.contains(name)) {
            return false;
        }

        const ParameterDefinition &def = m_parameterDefinitions.value(name);
        if (!validateValue(value, def)) {
            return false;
        }
    }

    return true;
}

bool QHYCCDDriver::commitParameters()
{
    QMutexLocker locker(&m_mutex);

    // Validate all pending parameters first
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
        if (!validateValue(value, def)) {
            emit errorOccurred(CameraError::makeError(
                CameraError::Code::CommitFailed,
                QString("Invalid value for parameter: %1").arg(name)));
            m_pendingParameters.clear();
            return false;
        }
    }

    // Apply parameters to camera
    for (auto it = m_pendingParameters.constBegin(); it != m_pendingParameters.constEnd(); ++it) {
        const QString &name = it.key();
        const QVariant &value = it.value();

        if (name == "exposure") {
            double exposureUs = value.toDouble() * 1000.0; // ms to us
            SetQHYCCDParam(m_cameraHandle, CONTROL_EXPOSURE, exposureUs);
        } else if (name == "gain") {
            SetQHYCCDParam(m_cameraHandle, CONTROL_GAIN, value.toDouble());
        } else if (name == "offset") {
            SetQHYCCDParam(m_cameraHandle, CONTROL_OFFSET, value.toDouble());
        } else if (name == "cooler_enabled") {
            if (value.toBool()) {
                double targetTemp = m_parameters.value("target_temperature", -10.0).toDouble();
                ControlQHYCCDTemp(m_cameraHandle, targetTemp);
            } else {
                SetQHYCCDParam(m_cameraHandle, CONTROL_MANULPWM, 0.0);
            }
        } else if (name == "binning") {
            int binFactor = value.toInt();
            // Set binning via SDK - both wbin and hbin
            uint32_t ret = SetQHYCCDBinMode(m_cameraHandle, binFactor, binFactor);
            if (ret != QHYCCD_SUCCESS) {
                emit errorOccurred(CameraError::makeError(
                    CameraError::Code::CommitFailed,
                    QString("Failed to set binning: %1").arg(ret)));
            }
        }
    }

    // Move pending to actual
    for (auto it = m_pendingParameters.constBegin(); it != m_pendingParameters.constEnd(); ++it) {
        m_parameters.insert(it.key(), it.value());
    }
    m_pendingParameters.clear();

    return true;
}

bool QHYCCDDriver::startCapture(int captureCount)
{
    QMutexLocker locker(&m_mutex);

    if (!m_connected.load()) {
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::NotConnected,
            "Cannot start capture: not connected to camera"));
        return false;
    }

    if (m_captureRunning.load()) {
        return true;
    }

    // Initialize frame buffer
    m_bufferSize = GetQHYCCDMemLength(m_cameraHandle);
    m_frameBuffer.resize(m_bufferSize);

    m_captureCount = captureCount;
    m_framesAcquired.store(0);
    m_captureRunning.store(true);

    m_captureThread = QThread::create([this]() { captureLoop(); });
    m_captureThread->start();

    m_state.store(CameraState::Acquiring);
    emit captureStarted(m_connectedCameraId);
    return true;
}

void QHYCCDDriver::stopCapture(int timeoutMs)
{
    QMutexLocker locker(&m_mutex);

    if (!m_captureRunning.load()) {
        return;
    }

    m_captureRunning.store(false);

    if (m_captureThread) {
        m_captureThread->quit();
        m_captureThread->wait(timeoutMs);
        delete m_captureThread;
        m_captureThread = nullptr;
    }

    if (m_state.load() == CameraState::Acquiring) {
        m_state.store(CameraState::Connected);
    }

    emit captureStopped(m_connectedCameraId);
}

CameraState QHYCCDDriver::state() const
{
    return m_state.load();
}

QString QHYCCDDriver::driverVersion() const
{
    return "1.0.0";
}

QString QHYCCDDriver::cameraId() const
{
    return m_connectedCameraId;
}

bool QHYCCDDriver::initSdk()
{
    if (s_sdkInitialized.load()) {
        s_sdkRefCount++;
        return true;
    }

    uint32_t ret = InitQHYCCDResource();
    if (ret != QHYCCD_SUCCESS) {
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::DriverError,
            QString("Failed to initialize QHYCCD SDK: %1").arg(ret)));
        return false;
    }

    s_sdkInitialized.store(true);
    s_sdkRefCount = 1;
    return true;
}

void QHYCCDDriver::releaseSdk()
{
    if (!s_sdkInitialized.load()) {
        return;
    }

    s_sdkRefCount--;
    if (s_sdkRefCount <= 0) {
        ReleaseQHYCCDResource();
        s_sdkInitialized.store(false);
        s_sdkRefCount = 0;
    }
}

void QHYCCDDriver::initializeParameterDefinitions()
{
    m_parameterDefinitions.clear();
    m_parameters.clear();
    m_pendingParameters.clear();

    // Get camera information
    char model[64] = {0};
    uint32_t ret = GetQHYCCDModel(m_connectedCameraId.toLatin1().data(), model);
    if (ret == QHYCCD_SUCCESS) {
        m_cameraModel = QString::fromLatin1(model);
    } else {
        m_cameraModel = "Unknown";
    }

    // Get sensor dimensions
    double chipWidth = 0, chipHeight = 0, pixelW = 0, pixelH = 0;
    uint32_t width = 0, height = 0;
    ret = GetQHYCCDChipInfo(m_cameraHandle, &chipWidth, &chipHeight,
                            &width, &height, &pixelW, &pixelH, &m_imageBytes);
    if (ret == QHYCCD_SUCCESS) {
        m_imageWidth = width;
        m_imageHeight = height;
    } else {
        m_imageWidth = 2048;
        m_imageHeight = 2048;
        m_imageBytes = 16;
    }
    if (ret != QHYCCD_SUCCESS) {
        m_imageWidth = 2048;
        m_imageHeight = 2048;
        m_imageBytes = 16;
    }

    ParameterDefinition param;

    // Exposure
    param = ParameterDefinition();
    param.name = "exposure";
    param.displayName = "Exposure Time";
    param.description = "Camera exposure time in milliseconds";
    param.category = ParameterCategory::Core;
    param.type = ParameterType::FloatRange;
    param.constraint.minValue = 0.001;
    param.constraint.maxValue = 10000.0;
    param.constraint.step = 0.001;
    param.constraint.unit = {"ms"};
    param.defaultValue = 100.0;
    param.order = 1.0f;
    m_parameterDefinitions.insert("exposure", param);
    m_parameters.insert("exposure", 100.0);

    // Gain
    param = ParameterDefinition();
    param.name = "gain";
    param.displayName = "Gain";
    param.description = "Camera analog gain";
    param.category = ParameterCategory::Core;
    param.type = ParameterType::FloatRange;
    param.constraint.minValue = 0.0;
    param.constraint.maxValue = 40.0;
    param.constraint.step = 0.1;
    param.defaultValue = 1.0;
    param.order = 2.0f;
    m_parameterDefinitions.insert("gain", param);
    m_parameters.insert("gain", 1.0);

    // Offset
    param = ParameterDefinition();
    param.name = "offset";
    param.displayName = "Offset";
    param.description = "Camera DC offset value";
    param.category = ParameterCategory::Core;
    param.type = ParameterType::FloatRange;
    param.constraint.minValue = 0.0;
    param.constraint.maxValue = 255.0;
    param.constraint.step = 1.0;
    param.defaultValue = 0.0;
    param.order = 3.0f;
    m_parameterDefinitions.insert("offset", param);
    m_parameters.insert("offset", 0.0);

    // ROI X
    param = ParameterDefinition();
    param.name = "roi_x";
    param.displayName = "ROI X Offset";
    param.description = "Region of interest X offset in pixels";
    param.category = ParameterCategory::Core;
    param.type = ParameterType::IntRange;
    param.constraint.minValue = 0;
    param.constraint.maxValue = static_cast<int>(m_imageWidth) - 1;
    param.constraint.step = 1;
    param.defaultValue = 0;
    param.isDynamic = true;
    param.order = 4.1f;
    m_parameterDefinitions.insert("roi_x", param);
    m_parameters.insert("roi_x", 0);

    // ROI Y
    param = ParameterDefinition();
    param.name = "roi_y";
    param.displayName = "ROI Y Offset";
    param.description = "Region of interest Y offset in pixels";
    param.category = ParameterCategory::Core;
    param.type = ParameterType::IntRange;
    param.constraint.minValue = 0;
    param.constraint.maxValue = static_cast<int>(m_imageHeight) - 1;
    param.constraint.step = 1;
    param.defaultValue = 0;
    param.isDynamic = true;
    param.order = 4.2f;
    m_parameterDefinitions.insert("roi_y", param);
    m_parameters.insert("roi_y", 0);

    // ROI Width
    param = ParameterDefinition();
    param.name = "roi_width";
    param.displayName = "ROI Width";
    param.description = "Region of interest width in pixels";
    param.category = ParameterCategory::Core;
    param.type = ParameterType::IntRange;
    param.constraint.minValue = 1;
    param.constraint.maxValue = static_cast<int>(m_imageWidth);
    param.constraint.step = 1;
    param.defaultValue = static_cast<int>(m_imageWidth);
    param.isDynamic = true;
    param.order = 4.3f;
    m_parameterDefinitions.insert("roi_width", param);
    m_parameters.insert("roi_width", static_cast<int>(m_imageWidth));

    // ROI Height
    param = ParameterDefinition();
    param.name = "roi_height";
    param.displayName = "ROI Height";
    param.description = "Region of interest height in pixels";
    param.category = ParameterCategory::Core;
    param.type = ParameterType::IntRange;
    param.constraint.minValue = 1;
    param.constraint.maxValue = static_cast<int>(m_imageHeight);
    param.constraint.step = 1;
    param.defaultValue = static_cast<int>(m_imageHeight);
    param.isDynamic = true;
    param.order = 4.4f;
    m_parameterDefinitions.insert("roi_height", param);
    m_parameters.insert("roi_height", static_cast<int>(m_imageHeight));

    // Binning
    param = ParameterDefinition();
    param.name = "binning";
    param.displayName = "Pixel Binning";
    param.description = "Hardware pixel binning factor";
    param.category = ParameterCategory::Core;
    param.type = ParameterType::IntCollection;
    param.constraint.validValues = {1, 2, 4, 8};
    param.defaultValue = 1;
    param.order = 5.0f;
    m_parameterDefinitions.insert("binning", param);
    m_parameters.insert("binning", 1);

    // Cooler Enabled
    param = ParameterDefinition();
    param.name = "cooler_enabled";
    param.displayName = "Cooling Enabled";
    param.description = "Enable thermoelectric cooling";
    param.category = ParameterCategory::Cooling;
    param.type = ParameterType::Boolean;
    param.defaultValue = false;
    param.order = 1.0f;
    m_parameterDefinitions.insert("cooler_enabled", param);
    m_parameters.insert("cooler_enabled", false);

    // Target Temperature
    param = ParameterDefinition();
    param.name = "target_temperature";
    param.displayName = "Target Temperature";
    param.description = "Target temperature for sensor cooling in Celsius";
    param.category = ParameterCategory::Cooling;
    param.type = ParameterType::FloatRange;
    param.constraint.minValue = -40.0;
    param.constraint.maxValue = 25.0;
    param.constraint.step = 1.0;
    param.defaultValue = -10.0;
    param.order = 2.0f;
    m_parameterDefinitions.insert("target_temperature", param);
    m_parameters.insert("target_temperature", -10.0);

    // Current Temperature (read-only, updated dynamically)
    param = ParameterDefinition();
    param.name = "current_temperature";
    param.displayName = "Current Temperature";
    param.description = "Current sensor temperature";
    param.category = ParameterCategory::Cooling;
    param.type = ParameterType::String;
    param.isReadOnly = true;
    param.isDynamic = true;
    param.isExtrinsic = true;
    param.order = 3.0f;
    param.defaultValue = "-";
    m_parameterDefinitions.insert("current_temperature", param);
    m_parameters.insert("current_temperature", param.defaultValue);

    // Camera Model (read-only info)
    param = ParameterDefinition();
    param.name = "camera_model";
    param.displayName = "Camera Model";
    param.description = "Camera model identifier";
    param.category = ParameterCategory::Info;
    param.type = ParameterType::String;
    param.isReadOnly = true;
    param.order = 1.0f;
    param.defaultValue = m_cameraModel;
    m_parameterDefinitions.insert("camera_model", param);
    m_parameters.insert("camera_model", m_cameraModel);

    // Serial Number (read-only info)
    param = ParameterDefinition();
    param.name = "serial_number";
    param.displayName = "Serial Number";
    param.description = "Camera serial number";
    param.category = ParameterCategory::Info;
    param.type = ParameterType::String;
    param.isReadOnly = true;
    param.order = 2.0f;
    param.defaultValue = m_connectedCameraId;
    m_parameterDefinitions.insert("serial_number", param);
    m_parameters.insert("serial_number", m_connectedCameraId);
}

void QHYCCDDriver::captureLoop()
{
    uint32_t width = 0, height = 0, bpp = 0, channels = 0;
    uint32_t ret = QHYCCD_ERROR;

    // Set up ROI if needed
    int roiX = m_parameters.value("roi_x", 0).toInt();
    int roiY = m_parameters.value("roi_y", 0).toInt();
    int roiWidth = m_parameters.value("roi_width", static_cast<int>(m_imageWidth)).toInt();
    int roiHeight = m_parameters.value("roi_height", static_cast<int>(m_imageHeight)).toInt();

    // Set ROI to camera using SetQHYCCDResolution
    SetQHYCCDResolution(m_cameraHandle, roiX, roiY, roiWidth, roiHeight);

    // Start live mode
    BeginQHYCCDLive(m_cameraHandle);

    while (m_captureRunning.load()) {
        ret = GetQHYCCDLiveFrame(m_cameraHandle, &width, &height, &bpp, &channels, m_frameBuffer.data());

        if (ret != QHYCCD_SUCCESS) {
            QThread::msleep(10);
            continue;
        }

        if (width > 0 && height > 0) {
            QImage image = convertBufferToImage(width, height, bpp, channels);

            quint64 timestamp = QDateTime::currentMSecsSinceEpoch() * 1000000; // us since epoch
            int frameNum = m_framesAcquired.fetch_add(1);

            emit frameReady(QSharedPointer<QImage>::create(image), timestamp, frameNum, m_connectedCameraId);

            // Check if we reached the capture count
            int count = m_captureCount.load();
            if (count > 0 && frameNum >= count - 1) {
                break;
            }
        }
    }

    StopQHYCCDLive(m_cameraHandle);
}

bool QHYCCDDriver::validateValue(const QVariant &value, const ParameterDefinition &def) const
{
    switch (def.type) {
    case ParameterType::FloatRange: {
        double val = value.toDouble();
        return val >= def.constraint.minValue && val <= def.constraint.maxValue;
    }
    case ParameterType::FloatCollection: {
        double val = value.toDouble();
        for (const QVariant &v : def.constraint.validValues) {
            if (qAbs(v.toDouble() - val) < 0.0001) return true;
        }
        return false;
    }
    case ParameterType::IntRange: {
        int val = value.toInt();
        return val >= def.constraint.minValue && val <= def.constraint.maxValue;
    }
    case ParameterType::IntCollection: {
        int val = value.toInt();
        for (const QVariant &v : def.constraint.validValues) {
            if (v.toInt() == val) return true;
        }
        return false;
    }
    case ParameterType::String:
        return value.canConvert<QString>();
    case ParameterType::StringCollection: {
        QString val = value.toString();
        for (const QVariant &v : def.constraint.validValues) {
            if (v.toString() == val) return true;
        }
        return false;
    }
    case ParameterType::Boolean:
        return value.canConvert<bool>();
    default:
        return true;
    }
}

QImage QHYCCDDriver::convertBufferToImage(uint32_t w, uint32_t h, uint32_t bpp, uint32_t channels)
{
    QImage image;

    if (bpp == 8 && channels == 1) {
        image = QImage(static_cast<const uchar*>(m_frameBuffer.data()), w, h, w, QImage::Format_Grayscale8);
    } else if (bpp == 16 && channels == 1) {
        image = QImage(static_cast<const uchar*>(m_frameBuffer.data()), w, h, w * 2, QImage::Format_Grayscale16);
    } else if (bpp == 24 && channels == 3) {
        image = QImage(static_cast<const uchar*>(m_frameBuffer.data()), w, h, w * 3, QImage::Format_RGB888);
    } else if (bpp == 32 && channels == 4) {
        image = QImage(static_cast<const uchar*>(m_frameBuffer.data()), w, h, w * 4, QImage::Format_ARGB32);
    } else {
        // Fallback: try to create a grayscale 16-bit image
        image = QImage(static_cast<const uchar*>(m_frameBuffer.data()), w, h, w * 2, QImage::Format_Grayscale16);
    }

    return image.copy(); // Return a deep copy so the buffer can be reused
}