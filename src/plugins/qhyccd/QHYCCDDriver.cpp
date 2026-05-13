#include "QHYCCDDriver.h"
#include "gui/DebugMacros.h"
#include "plugins/qhyccd/QHYCCDDriver.h"
#include <QDateTime>
#include <QDebug>
#include <cstdint>
#include <cstring>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <qvariant.h>

Q_LOGGING_CATEGORY(parameterCategory, "Parameter")
Q_LOGGING_CATEGORY(cameraCategory, "Camera")
Q_LOGGING_CATEGORY(configCategory, "Config")
Q_LOGGING_CATEGORY(displayCategory, "Display")
Q_LOGGING_CATEGORY(captureCategory, "Capture")
Q_LOGGING_CATEGORY(driverCategory, "Driver")

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
    if (numCameras == 0) {
        DRIVER_DEBUG << "No cameras found";
        return QStringList();
    }

    QStringList cameras;
    char id[64] = {0};
    for (uint32_t i = 0; i < numCameras; i++) {
        memset(id, 0, 64);
        uint32_t ret = GetQHYCCDId(i, id);
        if (ret == QHYCCD_SUCCESS) {
            DRIVER_DEBUG << "Found camera: " << id;
            cameras.append(QString::fromLatin1(id));
        }
    }

    return cameras;
}

bool QHYCCDDriver::connectToCamera(const QString &cameraId)
{
    QMutexLocker locker(&m_mutex);

    if (m_connected.load() && cameraId != m_connectedCameraId) {
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
    
    // set single mode
    uint32_t ret = SetQHYCCDStreamMode(m_cameraHandle, 0);
    if(ret != QHYCCD_SUCCESS){
        DRIVER_DEBUG << "Failed to set stream mode: " << ret;
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::DriverError,
            QString("Failed to set stream mode: %1").arg(ret)));
        return false;
    }

    // Set debayer off for RAW mode (must be after InitQHYCCD)
    ret = SetQHYCCDDebayerOnOff(m_cameraHandle, false);
    if (ret != QHYCCD_SUCCESS) {
        DRIVER_DEBUG << "Failed to set debayer off: " << ret;
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::DriverError,
            QString("Failed to set debayer off")));
        return false;
    }

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
    if (m_parameters.contains(name)) {
        return m_parameters.value(name);
    }
    return QVariant();
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

    if (!validate(value, def.constraint, def.type)) {
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

bool QHYCCDDriver::commitParameters()
{
    QMutexLocker locker(&m_mutex);

    // Validate all pending parameters first
    validateParameters();

    // Apply parameters to camera
    uint32_t ret = QHYCCD_ERROR;
    bool readModeChanged = m_pendingParameters.contains("read_mode");
    
    // If read mode changed, we need to reinit camera and reset all parameters.
    if(readModeChanged){
        QString modeName = m_pendingParameters.value("read_mode").toString();
        int modeIndex = m_readModeNames.indexOf(modeName);
        if(modeIndex < 0) modeIndex = 0;

        m_parameters.insert("read_mode", modeName);

        // set read mode
        ret = SetQHYCCDReadMode(m_cameraHandle, modeIndex);
        if(ret != QHYCCD_SUCCESS){
            DRIVER_DEBUG << "Failed to set read mode: " << ret;
            emit errorOccurred(CameraError::makeError(
                CameraError::Code::DriverError,
                QString("Failed to set read mode: %1").arg(ret)));
            return false;
        }

        // set stream mode
        uint32_t ret = SetQHYCCDStreamMode(m_cameraHandle, 0);
        if(ret != QHYCCD_SUCCESS){
            DRIVER_DEBUG << "Failed to set stream mode: " << ret;
            emit errorOccurred(CameraError::makeError(
                CameraError::Code::DriverError,
                QString("Failed to set stream mode: %1").arg(ret)));
            return false;
        }
        
        // reinit camera
        ret = InitQHYCCD(m_cameraHandle);
        if(ret != QHYCCD_SUCCESS){
            DRIVER_DEBUG << "Failed to reinitialize camera: " << ret;
            emit errorOccurred(CameraError::makeError(
                CameraError::Code::DriverError,
                QString("Failed to reinitialize camera: %1").arg(ret)));
            return false;
        }
        
        // reinit parameters
        for(auto it = m_parameterDefinitions.constBegin(); it != m_parameterDefinitions.constEnd(); ++it){
            const QString &name = it.key();
            const ParameterDefinition &def = it.value();
            if(name == "read_mode" || name == "stream_mode") continue; // already set above

            if(def.isReadOnly) continue;

            QVariant value = m_parameters.value(name, def.defaultValue);

            m_pendingParameters.insert(name, value);
        }
    }

    // roi validation and apply
    bool roiChanged = m_pendingParameters.contains("roi_x") || 
                      m_pendingParameters.contains("roi_y") ||
                      m_pendingParameters.contains("roi_width") || 
                      m_pendingParameters.contains("roi_height");
    
    if (roiChanged) {
        int binning = m_pendingParameters.contains("binning") 
            ? m_pendingParameters.value("binning").toInt()
            : m_parameters.value("binning", 1).toInt();
        int imageWidth = m_imageWidth / binning;
        int imageHeight = m_imageHeight / binning;
        
        int roiX = m_pendingParameters.contains("roi_x") 
            ? m_pendingParameters.value("roi_x").toInt()
            : m_parameters.value("roi_x", 0).toInt();
        int roiY = m_pendingParameters.contains("roi_y") 
            ? m_pendingParameters.value("roi_y").toInt()
            : m_parameters.value("roi_y", 0).toInt();
        int roiW = m_pendingParameters.contains("roi_width") 
            ? m_pendingParameters.value("roi_width").toInt()
            : m_parameters.value("roi_width", imageWidth).toInt();
        int roiH = m_pendingParameters.contains("roi_height") 
            ? m_pendingParameters.value("roi_height").toInt()
            : m_parameters.value("roi_height", imageHeight).toInt();
        
        // roi width must be even, if not, the data will be abnormal according to experiment
        if(roiW % 2 != 0){
            roiW++;
        }

        // check roi parameters are valid and do not exceed image bounds
        if (!(roiX >= 0 && roiX < imageWidth &&
              roiY >= 0 && roiY < imageHeight &&
              roiW >= 1 && roiW <= imageWidth &&
              roiH >= 1 && roiH <= imageHeight &&
              roiX + roiW <= imageWidth &&
              roiY + roiH <= imageHeight)) {
            CameraError::makeError(
                CameraError::Code::InvalidParameter,
                "roi parameters is invalid");
            DRIVER_DEBUG << "QHYCCDEZHAL: ROI validation failed - x:" << roiX << "y:" << roiY 
                       << "w:" << roiW << "h:" << roiH << "image:" << imageWidth << "x" << imageHeight;
            return false;
        }
        
        uint32_t ret = SetQHYCCDResolution(m_cameraHandle , roiX, roiY, roiW, roiH);
        if (ret != QHYCCD_SUCCESS) {
            CameraError::makeError(
                CameraError::Code::CommitFailed,
                QString("Failed to set camera roi, try to reconnect camera").arg(ret));
            return false;
        }
        m_parameters.insert("roi_x", roiX);
        m_parameters.insert("roi_y", roiY);
        m_parameters.insert("roi_width", roiW);
        m_parameters.insert("roi_height", roiH);
        
        // remove roi parameters from pending, as they have been applied
        m_pendingParameters.remove("roi_x");
        m_pendingParameters.remove("roi_y");
        m_pendingParameters.remove("roi_width");
        m_pendingParameters.remove("roi_height");
    }

    for (auto it = m_pendingParameters.constBegin(); it != m_pendingParameters.constEnd(); ++it) {
        const QString &name = it.key();
        const QVariant &value = it.value();
        uint32_t ret = QHYCCD_ERROR;

        if (name == "exposure") {
            double exposureUs = value.toDouble();
            ret = SetQHYCCDParam(m_cameraHandle, CONTROL_EXPOSURE, exposureUs);
        } else if (name == "gain") {
            ret = SetQHYCCDParam(m_cameraHandle, CONTROL_GAIN, value.toDouble());
        } else if (name == "offset") {
            ret = SetQHYCCDParam(m_cameraHandle, CONTROL_OFFSET, value.toDouble());
        } else if (name == "cooler_enabled") {
            if (value.toBool()) {
                double targetTemp = m_pendingParameters.contains("target_temperature")
                    ? m_pendingParameters.value("target_temperature").toDouble()
                    : m_parameters.value("target_temperature", -10.0).toDouble();
                ret = ControlQHYCCDTemp(m_cameraHandle, targetTemp);
            } else {
                ret = SetQHYCCDParam(m_cameraHandle, CONTROL_MANULPWM, 0.0);
            }
        } else if (name == "target_temperature"){
            bool cooler_enabled = m_parameters.value("cooler_enabled", false).toBool();
            if(cooler_enabled){
                ret = ControlQHYCCDTemp(m_cameraHandle, value.toDouble());
            }
        } else if (name == "usb_traffic"){
            int traffic = value.toInt();
            ret = SetQHYCCDParam(m_cameraHandle, CONTROL_USBTRAFFIC, traffic);
        } else if (name == "transfer_bit"){
            int traffic = value.toInt();
            ret = SetQHYCCDParam(m_cameraHandle, CONTROL_USBTRAFFIC, traffic);
        } else if (name == "binning") {
            int binFactor = value.toInt();
            // Set binning via SDK - both wbin and hbin
            ret = SetQHYCCDBinMode(m_cameraHandle, binFactor, binFactor);
        }

        if (ret != QHYCCD_SUCCESS) {
            emit errorOccurred(CameraError::makeError(
                CameraError::Code::CommitFailed,
                QString("Failed to set binning: %1").arg(ret)));
            m_pendingParameters.clear();
            return false;
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
        CancelQHYCCDExposingAndReadout(m_cameraHandle);
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
    if(m_cameraHandle == nullptr) {
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::DriverError,
            "Cannot initialize parameter definitions before connecting to camera"));
        return;
    }

    ParameterDefinition param;

    // cameraId
    param = ParameterDefinition();
    param.name = "serial_number";
    param.displayName = "Camera ID";
    param.description = "Camera serial number";
    param.category = ParameterCategory::Info;
    param.type = ParameterType::String;
    param.isReadOnly = true;
    param.order = 2.0f;
    param.defaultValue = m_connectedCameraId;
    m_parameterDefinitions.insert("serial_number", param);
    m_parameters.insert("serial_number", m_connectedCameraId);

    m_parameterDefinitions.clear();
    m_parameters.clear();
    m_pendingParameters.clear();

    // Get camera chip information
    char model[64] = {0};
    uint32_t ret = GetQHYCCDModel(m_connectedCameraId.toLatin1().data(), model);
    if (ret == QHYCCD_SUCCESS) {
        m_cameraModel = QString::fromLatin1(model);

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
    } else {
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::DriverError,
            QString("Failed to get camera model")));
        return;
    }

    // Get sensor dimensions
    ret = GetQHYCCDChipInfo(m_cameraHandle, &m_chipWidth, &m_chipHeight,
                            &m_imageWidth, &m_imageHeight, &m_pixelWidth, &m_pixelHeight, &m_imageBytes);
    if(ret == QHYCCD_SUCCESS){
        param = ParameterDefinition();
        param.name = "chipWidth";
        param.displayName = "Chip Width";
        param.description = "Sensor chip width in mm";
        param.category = ParameterCategory::Info;
        param.type = ParameterType::String;
        param.isReadOnly = true;
        param.order = 3.0f;
        param.defaultValue = static_cast<double>(m_chipWidth);
        m_parameterDefinitions.insert("chipWidth", param);
        m_parameters.insert("chipWidth", static_cast<double>(m_chipWidth));
        
        param = ParameterDefinition();
        param.name = "chipHeight";
        param.displayName = "Chip Height";
        param.description = "Sensor chip height in mm";
        param.category = ParameterCategory::Info;
        param.type = ParameterType::String;
        param.isReadOnly = true;
        param.order = 4.0f;
        param.defaultValue = static_cast<double>(m_chipHeight);
        m_parameterDefinitions.insert("chipHeight", param);
        m_parameters.insert("chipHeight", static_cast<double>(m_chipHeight));
        
        param = ParameterDefinition();
        param.name = "imageWidth";
        param.displayName = "Image Width";
        param.description = "Maximum image width in pixels";
        param.category = ParameterCategory::Info;
        param.type = ParameterType::String;
        param.isReadOnly = true;
        param.order = 5.0f;
        param.defaultValue = m_imageWidth;
        m_parameterDefinitions.insert("imageWidth", param);
        m_parameters.insert("imageWidth", m_imageWidth);
        
        param = ParameterDefinition();
        param.name = "imageHeight";
        param.displayName = "Image Height";
        param.description = "Maximum image height in pixels";
        param.category = ParameterCategory::Info;
        param.type = ParameterType::String;
        param.isReadOnly = true;
        param.order = 6.0f;
        param.defaultValue = m_imageHeight;
        m_parameterDefinitions.insert("imageHeight", param);
        m_parameters.insert("imageHeight", m_imageHeight);
        
        param = ParameterDefinition();
        param.name = "pixelWidth";
        param.displayName = "Pixel Width";
        param.description = "Pixel width in microns";
        param.category = ParameterCategory::Info;
        param.type = ParameterType::String;
        param.isReadOnly = true;
        param.order = 7.0f;
        param.defaultValue = m_pixelWidth;
        m_parameterDefinitions.insert("pixelWidth", param);
        m_parameters.insert("pixelWidth", m_pixelWidth);
        
        param = ParameterDefinition();
        param.name = "pixelHeight";
        param.displayName = "Pixel Height";
        param.description = "Pixel height in microns";
        param.category = ParameterCategory::Info;
        param.type = ParameterType::String;
        param.isReadOnly = true;
        param.order = 8.0f;
        param.defaultValue = m_pixelHeight;
        m_parameterDefinitions.insert("pixelHeight", param);
        m_parameters.insert("pixelHeight", m_pixelHeight);
        
        param = ParameterDefinition();
        param.name = "imageBytes";
        param.displayName = "Image Bytes";
        param.description = "Image buffer size in bytes";
        param.category = ParameterCategory::Info;
        param.type = ParameterType::String;
        param.isReadOnly = true;
        param.order = 9.0f;
        param.defaultValue = m_imageBytes;
        m_parameterDefinitions.insert("imageBytes", param);
        m_parameters.insert("imageBytes", m_imageBytes);
    }
    if (ret != QHYCCD_SUCCESS) {
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::DriverError,
            QString("Failed to get sensor dimensions")));
        return;
    }

    uint32_t numReadModes = 0;
    ret = GetQHYCCDNumberOfReadModes(m_cameraHandle, &numReadModes);
    qDebug() << "QHYCCDEZHAL: Number of read modes:" << numReadModes;

    m_readModeNames.clear();
    if (ret == QHYCCD_SUCCESS && numReadModes > 0) {
        char modeName[64] = {0};
        for (uint32_t i = 0; i < numReadModes; i++) {
            memset(modeName, 0, 64);
            ret = GetQHYCCDReadModeName(m_cameraHandle, i, modeName);
            if (ret == QHYCCD_SUCCESS) {
                m_readModeNames.append(QString::fromLatin1(modeName));
                qDebug() << "QHYCCDEZHAL: Read mode" << i << ":" << modeName;
            } else {
                emit errorOccurred(CameraError::makeError(
                    CameraError::Code::DriverError,
                    QString("Failed to get read mode name")));
                return;
            }
        }
    } else {
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::DriverError,
            QString("Failed to get read modes")));
        return;
    }

    // Exposure
    ret = IsQHYCCDControlAvailable(m_cameraHandle, CONTROL_EXPOSURE);
    if (ret == QHYCCD_SUCCESS) {
        param = ParameterDefinition();
        param.name = "exposure";
        param.displayName = "Exposure Time";
        param.description = "Camera exposure time";
        param.category = ParameterCategory::Core;
        param.type = ParameterType::FloatRange;
        
        // Query actual constraints from camera
        double minExp, maxExp, stepExp;
        ret = GetQHYCCDParamMinMaxStep(m_cameraHandle, CONTROL_EXPOSURE, &minExp, &maxExp, &stepExp);
        if (ret == QHYCCD_SUCCESS) {
            param.constraint.minValue = minExp;
            param.constraint.maxValue = maxExp;
            param.constraint.step = stepExp;
            qDebug() << "QHYCCDEZHAL: Exposure range from camera:" << minExp << "-" << maxExp << "step:" << stepExp;
        } else {
            emit errorOccurred(CameraError::makeError(
                CameraError::Code::DriverError,
                QString("Failed to get exposure range")));
        }
        
        param.constraint.unit = {"us", "ms", "s"};
        param.constraint.unitRange = {1000.0, 1000000.0};
        
        // Set default within valid range
        param.defaultValue = qBound(param.constraint.minValue, 100000.0, param.constraint.maxValue);
        param.order = 1.0f;
        m_parameterDefinitions.insert("exposure", param);
        m_parameters.insert("exposure", param.defaultValue);
    } else {
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::DriverError,
            QString("Failed to get exposure parameter")));
        return;
    }

    // Gain
    ret = IsQHYCCDControlAvailable(m_cameraHandle, CONTROL_GAIN);
    if (ret == QHYCCD_SUCCESS) {
        param = ParameterDefinition();
        param.name = "gain";
        param.displayName = "Gain";
        param.description = "Camera analog gain";
        param.category = ParameterCategory::Core;
        param.type = ParameterType::FloatRange;
        
        // Query actual constraints from camera
        double minGain, maxGain, stepGain;
        ret = GetQHYCCDParamMinMaxStep(m_cameraHandle, CONTROL_GAIN, &minGain, &maxGain, &stepGain);
        if (ret == QHYCCD_SUCCESS) {
            param.constraint.minValue = minGain;
            param.constraint.maxValue = maxGain;
            param.constraint.step = stepGain;
            qDebug() << "QHYCCDEZHAL: Gain range from camera:" << minGain << "-" << maxGain << "step:" << stepGain;
        } else {
            emit errorOccurred(CameraError::makeError(
                CameraError::Code::DriverError,
                QString("Failed to get gain range")));
        }
        
        // Set default within valid range
        param.defaultValue = qBound(param.constraint.minValue, 1.0, param.constraint.maxValue);
        param.order = 2.0f;
        m_parameterDefinitions.insert("gain", param);
        m_parameters.insert("gain", param.defaultValue);
    }

    // Offset
    ret = IsQHYCCDControlAvailable(m_cameraHandle, CONTROL_OFFSET);
    if (ret == QHYCCD_SUCCESS) {
        param = ParameterDefinition();
        param.name = "offset";
        param.displayName = "Offset";
        param.description = "Camera DC offset value";
        param.category = ParameterCategory::Core;
        param.type = ParameterType::FloatRange;
        
        // Query actual constraints from camera
        double minOffset, maxOffset, stepOffset;
        ret = GetQHYCCDParamMinMaxStep(m_cameraHandle, CONTROL_OFFSET, &minOffset, &maxOffset, &stepOffset);
        if (ret == QHYCCD_SUCCESS) {
            param.constraint.minValue = minOffset;
            param.constraint.maxValue = maxOffset;
            param.constraint.step = stepOffset;
            qDebug() << "QHYCCDEZHAL: Offset range from camera:" << minOffset << "-" << maxOffset << "step:" << stepOffset;
        } else {
            emit errorOccurred(CameraError::makeError(
                CameraError::Code::DriverError,
                QString("Failed to get offset range")));
        }
        
        param.defaultValue = qBound(param.constraint.minValue, 10.0, param.constraint.maxValue);
        param.order = 3.0f;
        m_parameterDefinitions.insert("offset", param);
        m_parameters.insert("offset", param.defaultValue);
    }

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
    ret = IsQHYCCDControlAvailable(m_cameraHandle,  CAM_BIN1X1MODE);
    param = ParameterDefinition();
    param.name = "binning";
    param.displayName = "Pixel Binning";
    param.description = "Hardware pixel binning factor";
    param.category = ParameterCategory::Core;
    param.type = ParameterType::IntCollection;
    param.order = 5.0f;
    if(ret == QHYCCD_SUCCESS){
        param.constraint.validValues.append(1);
        param.defaultValue = 1;
        m_parameterDefinitions.insert("binning", param);
        m_parameters.insert("binning", 1);
    }
    ret = IsQHYCCDControlAvailable(m_cameraHandle,  CAM_BIN2X2MODE);
    if(ret == QHYCCD_SUCCESS){
        param.constraint.validValues.append(2);
        m_parameterDefinitions.insert("binning", param);
        m_parameters.insert("binning", 1);
    }
    ret = IsQHYCCDControlAvailable(m_cameraHandle,  CAM_BIN3X3MODE);
    if(ret == QHYCCD_SUCCESS){
        param.constraint.validValues.append(3);
        m_parameterDefinitions.insert("binning", param);
        m_parameters.insert("binning", 1);
    }
    ret = IsQHYCCDControlAvailable(m_cameraHandle,  CAM_BIN4X4MODE);
    if(ret == QHYCCD_SUCCESS){
        param.constraint.validValues.append(4);
        m_parameterDefinitions.insert("binning", param);
        m_parameters.insert("binning", 1);
    }

    // USB Traffic - check availability and query constraints from camera
    ret = IsQHYCCDControlAvailable(m_cameraHandle, CONTROL_USBTRAFFIC);
    if (ret == QHYCCD_SUCCESS) {
        param = ParameterDefinition();
        param.name = "usb_traffic";
        param.displayName = "USB Traffic";
        param.description = "Adjust the frame rate in the continuous mode to obtain the maximum frame rate suitable for the computer. The larger the parameter value, the slower the frame rate.";
        param.category = ParameterCategory::Advanced;
        param.type = ParameterType::IntRange;
        
        // Query actual constraints from camera
        double minTraffic, maxTraffic, stepTraffic;
        ret = GetQHYCCDParamMinMaxStep(m_cameraHandle, CONTROL_USBTRAFFIC, &minTraffic, &maxTraffic, &stepTraffic);
        if (ret == QHYCCD_SUCCESS) {
            param.constraint.minValue = minTraffic;
            param.constraint.maxValue = maxTraffic;
            param.constraint.step = stepTraffic > 0 ? stepTraffic : 1;
            qDebug() << "QHYCCDEZHAL: USB Traffic range from camera:" << minTraffic << "-" << maxTraffic << "step:" << stepTraffic;
        } else {
            emit errorOccurred(CameraError::makeError(
                CameraError::Code::DriverError,
                QString("Failed to get USB traffic range")));
            return;
        }
        
        param.defaultValue = qBound(param.constraint.minValue, 16.0, param.constraint.maxValue);
        param.order = 1.0f;
        m_parameterDefinitions.insert("usb_traffic", param);
        m_parameters.insert("usb_traffic", param.defaultValue.toInt());
    }

    // read mode
    param = ParameterDefinition();
    param.name = "read_mode";
    param.displayName = "Read Mode";
    param.description = "Camera readout mode (affects speed/quality)";
    param.category = ParameterCategory::Core;
    param.type = ParameterType::StringCollection;
    for (const QString &name : m_readModeNames) {
        param.constraint.validValues.append(name);
    }
    param.defaultValue = m_readModeNames.isEmpty() ? "Default" : m_readModeNames.first();
    param.order = 5.0f;
    param.needReconnect = true;
    m_parameterDefinitions.insert("read_mode", param);
    m_parameters.insert("read_mode", param.defaultValue);

    // Transfer bit depth - check availability
    ret = IsQHYCCDControlAvailable(m_cameraHandle, CONTROL_TRANSFERBIT);
    if (ret == QHYCCD_SUCCESS) {
        param = ParameterDefinition();
        param.name = "transfer_bit";
        param.displayName = "Transfer Bit Depth";
        param.description = "Image bit depth (8 or 16)";
        param.category = ParameterCategory::Advanced;
        param.type = ParameterType::IntCollection;
        
        // Query actual constraints from camera to determine valid bit depths
        double minBits, maxBits, stepBits;
        ret = GetQHYCCDParamMinMaxStep(m_cameraHandle, CONTROL_TRANSFERBIT, &minBits, &maxBits, &stepBits);
        if (ret == QHYCCD_SUCCESS) {
            param.constraint.validValues.clear();
            for (int b = static_cast<int>(minBits); b <= static_cast<int>(maxBits); b += static_cast<int>(stepBits > 0 ? stepBits : 1)) {
                param.constraint.validValues.append(b);
            }
            qDebug() << "QHYCCDEZHAL: Transfer bit depths from camera:" << param.constraint.validValues;
        } else {
            emit errorOccurred(CameraError::makeError(
                CameraError::Code::DriverError,
                QString("Failed to get transfer bit depth range")));
            return;
        }
        
        // Set default to highest available bit depth
        int defaultBits = 16;
        if (!param.constraint.validValues.isEmpty() && param.constraint.validValues.contains(16)) {
            defaultBits = 16;
        } else if (!param.constraint.validValues.isEmpty()) {
            defaultBits = param.constraint.validValues.last().toInt();
        }
        param.defaultValue = defaultBits;
        param.order = 2.0f;
        m_parameterDefinitions.insert("transfer_bit", param);
        m_parameters.insert("transfer_bit", defaultBits);
    }

    // Cooler Enabled
    ret = IsQHYCCDControlAvailable(m_cameraHandle, CONTROL_COOLER);
    if (ret == QHYCCD_SUCCESS){
        param = ParameterDefinition();
        param.name = "cooler_enabled";
        param.displayName = "Cooler Enabled";
        param.description = "Enable camera cooler";
        param.category = ParameterCategory::Cooling;
        param.type = ParameterType::Boolean;
        param.defaultValue = false;
        param.order = 10.0f;
        m_parameterDefinitions.insert("cooler_enabled", param);
        m_parameters.insert("cooler_enabled", false);

        param = ParameterDefinition();
        param.name = "target_temperature";
        param.displayName = "Target Temperature";
        param.description = "Target sensor temperature in Celsius";
        param.category = ParameterCategory::Cooling;
        param.type = ParameterType::FloatRange;
        
        // Query actual constraints from camera
        double minTemp, maxTemp, stepTemp;
        ret = GetQHYCCDParamMinMaxStep(m_cameraHandle, CONTROL_COOLER, &minTemp, &maxTemp, &stepTemp);
        if (ret == QHYCCD_SUCCESS) {
            param.constraint.minValue = minTemp;
            param.constraint.maxValue = maxTemp;
            param.constraint.step = stepTemp > 0 ? stepTemp : 1.0;
            qDebug() << "QHYCCDEZHAL: Temperature range from camera:" << minTemp << "-" << maxTemp << "step:" << stepTemp;
        } else {
            emit errorOccurred(CameraError::makeError(
                CameraError::Code::DriverError,
                QString("Failed to get cooler temperature range")));
            return;
        }
        
        param.defaultValue = qBound(param.constraint.minValue, -10.0, param.constraint.maxValue);
        param.order = 11.0f;
        m_parameterDefinitions.insert("target_temperature", param);
        m_parameters.insert("target_temperature", param.defaultValue);
    }

    // Current temperature
    ret = IsQHYCCDControlAvailable(m_cameraHandle, CONTROL_CURTEMP);
    if (ret == QHYCCD_SUCCESS) {
        param = ParameterDefinition();
        param.name = "current_temperature";
        param.displayName = "Current Temperature";
        param.description = "Current sensor temperature in Celsius";
        param.category = ParameterCategory::Cooling;
        param.type = ParameterType::String;
        param.isReadOnly = true;
        param.isDynamic = true;
        param.isExtrinsic = true;
        param.order = 12.0f;
        param.defaultValue = 25;
        m_parameterDefinitions.insert("current_temperature", param);
        m_parameters.insert("current_temperature", 0.0);
    }

    // Humidity - check availability
    ret = IsQHYCCDControlAvailable(m_cameraHandle, CAM_HUMIDITY);
    if (ret == QHYCCD_SUCCESS) {
        double humidityValue = GetQHYCCDParam(m_cameraHandle, CAM_HUMIDITY);
        param = ParameterDefinition();
        param.name = "humidity";
        param.displayName = "Humidity";
        param.description = "Humidity in percent";
        param.category = ParameterCategory::Info;
        param.type = ParameterType::String;
        param.isReadOnly = true;
        param.isDynamic = true;
        param.isExtrinsic = true;
        param.order = 13.0f;
        param.defaultValue = humidityValue;
        m_parameterDefinitions.insert("humidity", param);
        m_parameters.insert("humidity", humidityValue);
    }
    
    // Pressure - check availability
    ret = IsQHYCCDControlAvailable(m_cameraHandle, CAM_PRESSURE);
    if (ret == QHYCCD_SUCCESS) {
        double pressureValue = GetQHYCCDParam(m_cameraHandle, CAM_PRESSURE);
        param = ParameterDefinition();
        param.name = "pressure";
        param.displayName = "Pressure";
        param.description = "Pressure in mbar";
        param.category = ParameterCategory::Info;
        param.type = ParameterType::String;
        param.isReadOnly = true;
        param.isDynamic = true;
        param.isExtrinsic = true;
        param.order = 14.0f;
        param.defaultValue = pressureValue;
        m_parameterDefinitions.insert("pressure", param);
        m_parameters.insert("pressure", pressureValue);
    }

    // Check effective area
    ret = GetQHYCCDEffectiveArea(m_cameraHandle, &m_effectiveStartX, &m_effectiveStartY, &m_effectiveWidth, &m_effectiveHeight);

    param = ParameterDefinition();
    param.name = "Effective Area start x";
    param.displayName = "Effective Area start x";
    param.description = "Effective area start x";
    param.category = ParameterCategory::Info;
    param.type = ParameterType::String;
    param.isReadOnly = true;
    param.order = 15.0f;
    param.defaultValue = m_effectiveStartX;
    m_parameterDefinitions.insert("effective_start_x", param);
    m_parameters.insert("effective_start_x", m_effectiveStartX);
    
    param = ParameterDefinition();
    param.name = "Effective Area start y";
    param.displayName = "Effective Area start y";
    param.description = "Effective area start y";
    param.category = ParameterCategory::Info;
    param.type = ParameterType::String;
    param.isReadOnly = true;
    param.order = 16.0f;
    param.defaultValue = m_effectiveStartY;
    m_parameterDefinitions.insert("effective_start_y", param);
    m_parameters.insert("effective_start_y", m_effectiveStartY);

    param = ParameterDefinition();
    param.name = "Effective Area width";
    param.displayName = "Effective Area width";
    param.description = "Effective area width";
    param.category = ParameterCategory::Info;
    param.type = ParameterType::String;
    param.isReadOnly = true;
    param.order = 17.0f;
    param.defaultValue = m_effectiveWidth;
    m_parameterDefinitions.insert("effective_width", param);
    m_parameters.insert("effective_width", m_effectiveWidth);

    param = ParameterDefinition();
    param.name = "Effective Area height";
    param.displayName = "Effective Area height";
    param.description = "Effective area height";
    param.category = ParameterCategory::Info;
    param.type = ParameterType::String;
    param.isReadOnly = true;
    param.order = 18.0f;
    param.defaultValue = m_effectiveHeight;
    m_parameterDefinitions.insert("effective_height", param);
    m_parameters.insert("effective_height", m_effectiveHeight);
}

void QHYCCDDriver::captureLoop()
{
    uint32_t width = 0, height = 0, bpp = 0, channels = 0;
    uint32_t ret = QHYCCD_ERROR;

    while (m_captureRunning.load()) {
        ExpQHYCCDSingleFrame(m_cameraHandle);
        ret = GetQHYCCDSingleFrame(m_cameraHandle, &width, &height, &bpp, &channels, m_frameBuffer.data());

        if (ret != QHYCCD_SUCCESS) {
            DRIVER_DEBUG << "Failed to get frame (" << m_framesAcquired << "): " << ret;
            emit errorOccurred(CameraError::makeError(
                CameraError::Code::CaptureFailed,
                QString("Failed to get frame").arg(ret)));
            break;
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
    
    CancelQHYCCDExposingAndReadout(m_cameraHandle);
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