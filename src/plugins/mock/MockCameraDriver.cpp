#include "MockCameraDriver.h"

#include <QDateTime>
#include <QThread>
#include <QRandomGenerator>
#include <QDebug>
#include <cmath>
#include <cstring>

MockCameraDriver::MockCameraDriver(QObject *parent)
    : ICameraDriver(parent)
{
    m_captureTimer = new QTimer(this);
    m_captureTimer->setSingleShot(false);
    connect(m_captureTimer, &QTimer::timeout, this, &MockCameraDriver::onCaptureTimer);
}

MockCameraDriver::~MockCameraDriver()
{
    m_capturing.store(false);
    m_state.store(CameraState::Disconnected);
}

QStringList MockCameraDriver::enumerate()
{
    return QStringList() << "mock-001" << "mock-002" << "mock-003";
}

// TODO: 连接加一点延时(2s)以模拟真实的连接过程
bool MockCameraDriver::connectToCamera(const QString &cameraId)
{
    QMutexLocker locker(&m_mutex);

    QStringList available = enumerate();
    if (!available.contains(cameraId)) {
        m_lastError = CameraError::makeError(
            CameraError::Code::InvalidParameter,
            QString("Invalid camera ID: %1").arg(cameraId));
        emit errorOccurred(m_lastError);
        return false;
    }

    if (m_state.load() == CameraState::Connected) {
        disconnectCamera();
    }

    m_state.store(CameraState::Connecting);
    m_connectedCameraId = cameraId;

    m_parameterDefinitions.clear();
    m_parameters.clear();
    m_pendingParameters.clear();

    initializeParameterDefinitions(cameraId);

    m_state.store(CameraState::Connected);
    m_frameNumber.store(0);
    m_framesAcquired.store(0);
    m_currentSensorTemp = 25.0;
    m_currentHeatsinkTemp = 25.0;

    emit connectionChanged(true, cameraId);
    return true;
}

void MockCameraDriver::disconnectCamera()
{
    QMutexLocker locker(&m_mutex);

    if (m_state.load() == CameraState::Disconnected) {
        return;
    }

    stopCapture(100);

    m_parameters.clear();
    m_parameterDefinitions.clear();
    m_pendingParameters.clear();

    QString oldCameraId = m_connectedCameraId;
    m_connectedCameraId.clear();
    m_state.store(CameraState::Disconnected);

    emit connectionChanged(false, oldCameraId);
}

bool MockCameraDriver::isConnected() const
{
    return m_state.load() == CameraState::Connected;
}

QStringList MockCameraDriver::parameterNames() const
{
    QMutexLocker locker(&m_mutex);
    return m_parameterDefinitions.keys();
}

ParameterDefinition MockCameraDriver::parameter(const QString &name) const
{
    QMutexLocker locker(&m_mutex);
    if (m_parameterDefinitions.contains(name)) {
        return m_parameterDefinitions.value(name);
    }
    return ParameterDefinition();
}

QVariant MockCameraDriver::parameterValue(const QString &name) const
{
    QMutexLocker locker(&m_mutex);
    if (m_pendingParameters.contains(name)) {
        return m_pendingParameters.value(name);
    }
    return m_parameters.value(name);
}

bool MockCameraDriver::setParameter(const QString &name, const QVariant &value)
{
    QMutexLocker locker(&m_mutex);

    if (!m_parameterDefinitions.contains(name)) {
        m_lastError = CameraError::makeError(
            CameraError::Code::InvalidParameter,
            QString("Unknown parameter: %1").arg(name));
        emit errorOccurred(m_lastError);
        return false;
    }

    const ParameterDefinition &def = m_parameterDefinitions.value(name);

    if (def.isReadOnly) {
        return true;
    }

    if (!validateValue(value, def)) {
        m_lastError = CameraError::makeError(
            CameraError::Code::ValueOutOfRange,
            QString("Invalid value for parameter: %1").arg(name));
        emit errorOccurred(m_lastError);
        return false;
    }

    m_pendingParameters.insert(name, value);
    return true;
}

bool MockCameraDriver::validateParameters()
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

// TODO: 设置参数加一点延时(1s)以模拟真实的设置过程
bool MockCameraDriver::commitParameters()
{
    QMutexLocker locker(&m_mutex);

    for (auto it = m_pendingParameters.constBegin(); it != m_pendingParameters.constEnd(); ++it) {
        const QString &name = it.key();
        const QVariant &value = it.value();

        if (!m_parameterDefinitions.contains(name)) {
            m_lastError = CameraError::makeError(
                CameraError::Code::CommitFailed,
                "Parameter validation failed during commit");
            emit errorOccurred(m_lastError);
            m_pendingParameters.clear();
            return false;
        }

        const ParameterDefinition &def = m_parameterDefinitions.value(name);
        if (!validateValue(value, def)) {
            m_lastError = CameraError::makeError(
                CameraError::Code::CommitFailed,
                QString("Invalid value for parameter: %1").arg(name));
            emit errorOccurred(m_lastError);
            m_pendingParameters.clear();
            return false;
        }

        // FIXME: 将这两个参数纳入到 m_parameters 中统一管理
        if (name == "pattern_type") {
            m_patternType = value.toInt();
        } else if (name == "gain") {
            m_gain = value.toDouble();
        }
    }

    m_parameters.insert(m_pendingParameters);
    m_pendingParameters.clear();

    return true;
}

bool MockCameraDriver::startCapture(int captureCount)
{
    QMutexLocker locker(&m_mutex);

    if (m_state.load() != CameraState::Connected) {
        m_lastError = CameraError::makeError(
            CameraError::Code::NotConnected,
            "Cannot start capture: not connected to camera");
        emit errorOccurred(m_lastError);
        return false;
    }

    if (m_capturing.load()) {
        return true;
    }

    m_captureCount = captureCount;
    m_framesAcquired.store(0);
    m_capturing.store(true);
    m_state.store(CameraState::Acquiring);

    double exposureMs = m_parameters.value("exposure", 100.0).toDouble();
    int intervalMs = static_cast<int>(exposureMs);
    if (intervalMs < 10) {
        intervalMs = 10;
    }
    m_captureTimer->start(intervalMs);

    emit captureStarted(m_connectedCameraId);
    return true;
}

void MockCameraDriver::stopCapture(int timeoutMs)
{
    QMutexLocker locker(&m_mutex);

    if (!m_capturing.load()) {
        return;
    }

    m_captureTimer->stop();
    m_capturing.store(false);

    if (m_state.load() == CameraState::Acquiring) {
        m_state.store(CameraState::Connected);
    }

    bool wasAutoStop = m_autoStop;
    m_autoStop = false;

    if (!wasAutoStop) {
        updateTemperatures();
        QImage image = generateFrame();
        int frameNum = ++m_frameNumber;
        quint64 timestamp = QDateTime::currentMSecsSinceEpoch() * 1000;
        QSharedPointer<QImage> imagePtr = QSharedPointer<QImage>::create(image);
        emit frameReady(imagePtr, timestamp, frameNum, m_connectedCameraId);
    }

    emit captureStopped(m_connectedCameraId);
}

CameraState MockCameraDriver::state() const
{
    return m_state.load();
}

QString MockCameraDriver::driverVersion() const
{
    return "1.0.0";
}

QString MockCameraDriver::cameraId() const
{
    return m_connectedCameraId;
}

void MockCameraDriver::onCaptureTimer()
{
    if (!m_capturing.load()) {
        m_captureTimer->stop();
        return;
    }

    int captureCount = m_captureCount;
    int framesAcquired = m_framesAcquired.load();

    if (captureCount > 0 && framesAcquired >= captureCount) {
        m_autoStop = true;
        stopCapture(100);
        return;
    }

    QMutexLocker locker(&m_mutex);

    updateTemperatures();

    QImage image = generateFrame();

    int frameNum = ++m_frameNumber;
    m_framesAcquired.store(framesAcquired + 1);

    quint64 timestamp = QDateTime::currentMSecsSinceEpoch() * 1000;

    QSharedPointer<QImage> imagePtr = QSharedPointer<QImage>::create(image);

    emit frameReady(imagePtr, timestamp, frameNum, m_connectedCameraId);
}

void MockCameraDriver::initializeParameterDefinitions(const QString &cameraId)
{
    int maxWidth = 2048;
    int maxHeight = 2048;
    double maxExposure = 10000.0;
    bool supportsCooling = true;
    double minCoolingTemp = -40.0;
    double maxCoolingTemp = 25.0;
    QList<int> binningFactors = {1, 2, 4, 8};

    if (cameraId == "mock-002") {
        maxWidth = 4096;
        maxHeight = 4096;
        minCoolingTemp = -50.0;
    } else if (cameraId == "mock-003") {
        maxWidth = 1024;
        maxHeight = 1024;
        binningFactors = {1, 2};
        minCoolingTemp = -70.0;
        maxCoolingTemp = -5.0;
    }

    ParameterDefinition param;

    param = ParameterDefinition();
    param.name = "exposure";
    param.displayName = "Exposure Time";
    param.description = "Camera exposure time in milliseconds";
    param.category = ParameterCategory::Core;
    param.type = ParameterType::FloatRange;
    param.constraint.minValue = 1.0;
    param.constraint.maxValue = maxExposure;
    param.constraint.step = 1.0;
    param.constraint.unit = {"ms"}; // FIXME: unit 只有 ms，需要一个更长的单位 "s"， 方便设置更长的曝光时间。需要同步添加 unitRange 来支持单位转换。查看 ParameterConstraint 中的 unit 和 unitRange 的设计
    param.defaultValue = 100.0;
    param.order = 1.0f;
    m_parameterDefinitions.insert("exposure", param);
    m_parameters.insert("exposure", 100.0);

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

    param = ParameterDefinition();
    param.name = "roi_x";
    param.displayName = "ROI X Offset";
    param.description = "Region of interest X offset in pixels";
    param.category = ParameterCategory::Core;
    param.type = ParameterType::IntRange;
    param.constraint.minValue = 0;
    param.constraint.maxValue = maxWidth - 1;
    param.constraint.step = 1;
    param.defaultValue = 0;
    param.isDynamic = true;
    param.order = 4.1f;
    m_parameterDefinitions.insert("roi_x", param);
    m_parameters.insert("roi_x", 0);

    param = ParameterDefinition();
    param.name = "roi_y";
    param.displayName = "ROI Y Offset";
    param.description = "Region of interest Y offset in pixels";
    param.category = ParameterCategory::Core;
    param.type = ParameterType::IntRange;
    param.constraint.minValue = 0;
    param.constraint.maxValue = maxHeight - 1;
    param.constraint.step = 1;
    param.defaultValue = 0;
    param.isDynamic = true;
    param.order = 4.2f;
    m_parameterDefinitions.insert("roi_y", param);
    m_parameters.insert("roi_y", 0);

    param = ParameterDefinition();
    param.name = "roi_width";
    param.displayName = "ROI Width";
    param.description = "Region of interest width in pixels";
    param.category = ParameterCategory::Core;
    param.type = ParameterType::IntRange;
    param.constraint.minValue = 1;
    param.constraint.maxValue = maxWidth;
    param.constraint.step = 1;
    param.defaultValue = maxWidth;
    param.isDynamic = true;
    param.order = 4.3f;
    m_parameterDefinitions.insert("roi_width", param);
    m_parameters.insert("roi_width", maxWidth);

    param = ParameterDefinition();
    param.name = "roi_height";
    param.displayName = "ROI Height";
    param.description = "Region of interest height in pixels";
    param.category = ParameterCategory::Core;
    param.type = ParameterType::IntRange;
    param.constraint.minValue = 1;
    param.constraint.maxValue = maxHeight;
    param.constraint.step = 1;
    param.defaultValue = maxHeight;
    param.isDynamic = true;
    param.order = 4.4f;
    m_parameterDefinitions.insert("roi_height", param);
    m_parameters.insert("roi_height", maxHeight);

    param = ParameterDefinition();
    param.name = "binning";
    param.displayName = "Pixel Binning";
    param.description = "Hardware pixel binning factor";
    param.category = ParameterCategory::Core;
    param.type = ParameterType::IntCollection;
    for (int v : binningFactors) {
        param.constraint.validValues.append(v);
    }
    param.defaultValue = 1;
    param.order = 5.0f;
    m_parameterDefinitions.insert("binning", param);
    m_parameters.insert("binning", 1);

    param = ParameterDefinition();
    param.name = "vertical_binning";
    param.displayName = "Vertical Binning";
    param.description = "Enable vertical binning";
    param.category = ParameterCategory::Core;
    param.type = ParameterType::Boolean;
    param.defaultValue = false;
    param.order = 6.0f;
    m_parameterDefinitions.insert("vertical_binning", param);
    m_parameters.insert("vertical_binning", false);

    if (supportsCooling) {
        param = ParameterDefinition();
        param.name = "cooling_enabled";
        param.displayName = "Cooling Enabled";
        param.description = "Enable thermoelectric cooling";
        param.category = ParameterCategory::Cooling;
        param.type = ParameterType::Boolean;
        param.defaultValue = false;
        param.order = 1.0f;
        m_parameterDefinitions.insert("cooling_enabled", param);
        m_parameters.insert("cooling_enabled", false);

        param = ParameterDefinition();
        param.name = "cooling_target_temp";
        param.displayName = "Cooling Target Temperature";
        param.description = "Target temperature for sensor cooling in Celsius";
        param.category = ParameterCategory::Cooling;
        param.type = ParameterType::FloatRange;
        param.constraint.minValue = minCoolingTemp;
        param.constraint.maxValue = maxCoolingTemp;
        param.constraint.step = 1.0;
        param.defaultValue = -10.0;
        param.order = 2.0f;
        m_parameterDefinitions.insert("cooling_target_temp", param);
        m_parameters.insert("cooling_target_temp", -10.0);

        param = ParameterDefinition();
        param.name = "cooling_sensor_temp";
        param.displayName = "Sensor Temperature";
        param.description = "Current sensor die temperature";
        param.category = ParameterCategory::Cooling;
        param.type = ParameterType::String;
        param.isReadOnly = true;
        param.isDynamic = true;
        param.isExtrinsic = true;
        param.order = 3.0f;
        param.defaultValue = "-";
        m_parameterDefinitions.insert("cooling_sensor_temp", param);
        m_parameters.insert("cooling_sensor_temp", param.defaultValue);

        param = ParameterDefinition();
        param.name = "cooling_heatsink_temp";
        param.displayName = "Heatsink Temperature";
        param.description = "Current heatsink temperature";
        param.category = ParameterCategory::Cooling;
        param.type = ParameterType::String;
        param.isReadOnly = true;
        param.isDynamic = true;
        param.isExtrinsic = true;
        param.order = 4.0f;
        param.defaultValue = "-";
        m_parameterDefinitions.insert("cooling_heatsink_temp", param);
        m_parameters.insert("cooling_heatsink_temp", param.defaultValue);
    }

    // FIXME: Info 类型的 paramter type 应该定义为 string，它们是展示用的，不需要参与计算和验证
    param = ParameterDefinition();
    param.name = "sensor_width";
    param.displayName = "Sensor Width";
    param.description = "Total sensor width in pixels";
    param.category = ParameterCategory::Info;
    param.type = ParameterType::IntRange;
    param.constraint.minValue = 1;
    param.constraint.maxValue = maxWidth;
    param.isReadOnly = true;
    param.order = 1.0f;
    param.defaultValue = maxWidth;
    m_parameterDefinitions.insert("sensor_width", param);
    m_parameters.insert("sensor_width", maxWidth);

    param = ParameterDefinition();
    param.name = "sensor_height";
    param.displayName = "Sensor Height";
    param.description = "Total sensor height in pixels";
    param.category = ParameterCategory::Info;
    param.type = ParameterType::IntRange;
    param.constraint.minValue = 1;
    param.constraint.maxValue = maxHeight;
    param.isReadOnly = true;
    param.order = 2.0f;
    param.defaultValue = maxHeight;
    m_parameterDefinitions.insert("sensor_height", param);
    m_parameters.insert("sensor_height", maxHeight);

    param = ParameterDefinition();
    param.name = "bit_depth";
    param.displayName = "Bit Depth";
    param.description = "ADC bit depth for pixel data";
    param.category = ParameterCategory::Info;
    param.type = ParameterType::String;
    param.isReadOnly = true;
    param.order = 3.0f;
    param.defaultValue = "16-bit";
    m_parameterDefinitions.insert("bit_depth", param);
    m_parameters.insert("bit_depth", param.defaultValue);

    param = ParameterDefinition();
    param.name = "camera_model";
    param.displayName = "Camera Model";
    param.description = "Camera model identifier";
    param.category = ParameterCategory::Info;
    param.type = ParameterType::String;
    param.isReadOnly = true;
    param.order = 4.0f;
    param.defaultValue = "MockCamera-" + cameraId;
    m_parameterDefinitions.insert("camera_model", param);
    m_parameters.insert("camera_model", param.defaultValue);

    param = ParameterDefinition();
    param.name = "pattern_type";
    param.displayName = "Pattern Type";
    param.description = "Image pattern type: 0=gradient, 1=noise, 2=interference, 3=fastfill";
    param.category = ParameterCategory::Advanced;
    param.type = ParameterType::IntRange;
    param.constraint.minValue = 0;
    param.constraint.maxValue = 3;
    param.constraint.step = 1;
    param.defaultValue = 0;
    param.order = 1.0f;
    m_parameterDefinitions.insert("pattern_type", param);
    m_parameters.insert("pattern_type", 0);
}

QImage MockCameraDriver::generateFrame()
{
    int binningFactor = m_parameters.value("binning").toInt();
    int roiX = m_parameters.value("roi_x").toInt();
    int roiY = m_parameters.value("roi_y").toInt();
    int roiWidth = m_parameters.value("roi_width").toInt();
    int roiHeight = m_parameters.value("roi_height").toInt();
    bool verticalBinning = m_parameters.value("vertical_binning").toBool();

    int maxWidth = m_parameters.value("sensor_width", 2048).toInt();
    int maxHeight = m_parameters.value("sensor_height", 2048).toInt();

    int fullWidth = maxWidth / binningFactor;
    int fullHeight = maxHeight / binningFactor;

    if (verticalBinning) {
        fullHeight = 1;
    }

    QImage image;
    switch (m_patternType) {
    case 1:
        image = generateNoiseImage(fullWidth, fullHeight);
        break;
    case 2:
        image = generateDoubleSlitInterferenceImage(fullWidth, fullHeight);
        break;
    case 3:
        image = generateFastFillImage(fullWidth, fullHeight);
        break;
    default:
        image = generateGradientImage(fullWidth, fullHeight);
        break;
    }

    if (roiWidth > 0 && roiHeight > 0 && !verticalBinning) {
        int cropX = qBound(0, roiX, fullWidth - 1);
        int cropY = qBound(0, roiY, fullHeight - 1);
        int cropWidth = qMin(roiWidth, fullWidth - cropX);
        int cropHeight = qMin(roiHeight, fullHeight - cropY);
        image = image.copy(cropX, cropY, cropWidth, cropHeight);
    } else if (verticalBinning) {
        image = image.copy(0, 0, fullWidth, 1);
    }

    return image;
}

QImage MockCameraDriver::generateGradientImage(int width, int height)
{
    QImage image(width, height, QImage::Format_Grayscale16);

    if (width <= 0 || height <= 0) {
        return image;
    }

    double gainFactor = 1.0 + m_gain / 20.0;

    quint16 *firstRow = reinterpret_cast<quint16*>(image.scanLine(0));
    for (int x = 0; x < width; ++x) {
        double signal = (static_cast<double>(x) / width) * 65535.0 * gainFactor;
        firstRow[x] = static_cast<quint16>(qBound(0.0, signal, 65535.0));
    }

    for (int y = 1; y < height; ++y) {
        quint16 *row = reinterpret_cast<quint16*>(image.scanLine(y));
        memcpy(row, firstRow, width * sizeof(quint16));
    }

    return image;
}

QImage MockCameraDriver::generateNoiseImage(int width, int height)
{
    QImage image(width, height, QImage::Format_Grayscale16);

    if (width <= 0 || height <= 0) {
        return image;
    }

    double gainFactor = 1.0 + m_gain / 20.0;

    for (int y = 0; y < height; ++y) {
        quint16 *line = reinterpret_cast<quint16*>(image.scanLine(y));
        for (int x = 0; x < width; ++x) {
            int value = QRandomGenerator::global()->bounded(65536);
            value = static_cast<int>(value * gainFactor);
            line[x] = static_cast<quint16>(qBound(0, value, 65535));
        }
    }

    return image;
}

QImage MockCameraDriver::generateDoubleSlitInterferenceImage(int width, int height)
{
    QImage image(width, height, QImage::Format_Grayscale16);

    if (width <= 0 || height <= 0) {
        return image;
    }

    const double gainFactor = 1.0 + m_gain / 20.0;
    const double darkCurrent = 10.0;
    const double pixelSize = 10e-6;
    const double screenDistance = 50e-3;
    const double wavelength = 550e-9;
    const double slitSeparation = 100e-6;
    const double slitWidth = 20e-6;
    const double interferenceScale = M_PI * slitSeparation / (wavelength * screenDistance);
    const double diffractionScale = M_PI * slitWidth / (wavelength * screenDistance);
    const int centerX = width / 2;
    const int centerY = height / 2;

    int maxRadius = qMin(width, height) / 2 + 1;
    std::vector<quint16> radialLookup(maxRadius);

    for (int r = 0; r < maxRadius; ++r) {
        double physicalR = r * pixelSize;
        double sinTheta = physicalR / screenDistance;

        double intPhase = interferenceScale * physicalR;
        double interference = std::cos(intPhase);
        double intensity = interference * interference;

        double diffPhase = diffractionScale * physicalR;
        double diffraction;
        if (std::abs(diffPhase) < 1e-10) {
            diffraction = 1.0;
        } else {
            double sinc = std::sin(diffPhase) / diffPhase;
            diffraction = sinc * sinc;
        }

        double signal = intensity * diffraction * 65535.0 * gainFactor * 0.5 + darkCurrent;
        radialLookup[r] = static_cast<quint16>(qBound(0.0, signal, 65535.0));
    }

    for (int y = 0; y < height; ++y) {
        quint16 *line = reinterpret_cast<quint16*>(image.scanLine(y));
        double dy = y - centerY;
        double dy2 = dy * dy;

        for (int x = 0; x < width; ++x) {
            double dx = x - centerX;
            int r = static_cast<int>(std::sqrt(dx * dx + dy2));
            r = qMin(r, maxRadius - 1);

            int noise = static_cast<int>((QRandomGenerator::global()->generateDouble() - 0.5) * 50);
            int value = static_cast<int>(radialLookup[r]) + noise;
            line[x] = static_cast<quint16>(qBound(0, value, 65535));
        }
    }

    return image;
}

QImage MockCameraDriver::generateFastFillImage(int width, int height)
{
    QImage image(width, height, QImage::Format_Grayscale16);

    if (width <= 0 || height <= 0) {
        return image;
    }

    double gainFactor = 1.0 + m_gain / 20.0;

    quint16 *firstRow = reinterpret_cast<quint16*>(image.scanLine(0));
    for (int x = 0; x < width; ++x) {
        int colOffset = x * 65535 / (width > 1 ? width - 1 : 1);
        double scaledValue = colOffset * gainFactor;
        firstRow[x] = static_cast<quint16>(qBound(0.0, scaledValue, 65535.0));
    }

    for (int y = 1; y < height; ++y) {
        quint16 *line = reinterpret_cast<quint16*>(image.scanLine(y));
        int rowOffset = y * 65535 / (height > 1 ? height - 1 : 1);

        for (int x = 0; x < width; ++x) {
            int colOffset = x * 65535 / (width > 1 ? width - 1 : 1);
            double scaledValue = ((rowOffset + colOffset) / 2.0) * gainFactor;
            line[x] = static_cast<quint16>(qBound(0.0, scaledValue, 65535.0));
        }
    }

    return image;
}

bool MockCameraDriver::validateValue(const QVariant &value, const ParameterDefinition &def) const
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

// FIXME: updateTemperatures 应该使用一个独立的计时器定时更新，而不是在 capture timer 中更新，这样可以更真实地模拟温度变化的过程，并且不会受到 capture timer 频率的影响。
void MockCameraDriver::updateTemperatures()
{
    bool coolingEnabled = m_parameters.value("cooling_enabled", false).toBool();
    if (!coolingEnabled) {
        m_currentSensorTemp += (25.0 - m_currentSensorTemp) * 0.05;
        m_currentHeatsinkTemp += (25.0 - m_currentHeatsinkTemp) * 0.05;
    } else {
        double targetTemp = m_parameters.value("cooling_target_temp", -10.0).toDouble();
        m_currentSensorTemp += (targetTemp - m_currentSensorTemp) * 0.1;
        m_currentHeatsinkTemp += ((targetTemp + 15.0) - m_currentHeatsinkTemp) * 0.1;
    }

    m_parameters.insert("cooling_sensor_temp", QString::number(m_currentSensorTemp, 'f', 1));
    m_parameters.insert("cooling_heatsink_temp", QString::number(m_currentHeatsinkTemp, 'f', 1));
}