#include "PicamDriver.h"
#include "gui/DebugMacros.h"

#include <QThread>
#include <QDebug>
#include <QDateTime>

Q_LOGGING_CATEGORY(parameterCategory, "Parameter")
Q_LOGGING_CATEGORY(cameraCategory, "Camera")
Q_LOGGING_CATEGORY(configCategory, "Config")
Q_LOGGING_CATEGORY(displayCategory, "Display")
Q_LOGGING_CATEGORY(captureCategory, "Capture")
Q_LOGGING_CATEGORY(driverCategory, "Driver")

namespace {

struct PicamParameterMetadata {
    QString displayName;
    QString description;
    QVariant defaultValue;
    float order;
};

PicamParameterMetadata picamParameterMetadata(PicamParameter param)
{
    using M = PicamParameterMetadata;
    switch (param) {
    case PicamParameter_ExposureTime:
        return M{QStringLiteral("Exposure Time"),
                 QStringLiteral("Camera exposure time in seconds"),
                 QVariant(0.1), 100.0f};
    case PicamParameter_AdcBitDepth:
        return M{QStringLiteral("ADC Bit Depth"),
                 QStringLiteral("ADC conversion bit depth"),
                 QVariant(0), 250.0f};
    case PicamParameter_AdcAnalogGain:
        return M{QStringLiteral("Analog Gain"),
                 QStringLiteral("Camera analog gain setting"),
                 QVariant(), 300.0f};
    case PicamParameter_AdcSpeed:
        return M{QStringLiteral("ADC Speed"),
                 QStringLiteral("ADC readout speed"),
                 QVariant(), 400.0f};
    case PicamParameter_AdcQuality:
        return M{QStringLiteral("ADC Quality"),
                 QStringLiteral("ADC quality vs speed tradeoff"),
                 QVariant(), 500.0f};
    case PicamParameter_PixelFormat:
        return M{QStringLiteral("Pixel Format"),
                 QStringLiteral("Image pixel format"),
                 QVariant(), 600.0f};
    case PicamParameter_PixelBitDepth:
        return M{QStringLiteral("Pixel Bit Depth"),
                 QStringLiteral("Image bit depth"),
                 QVariant(0), 68.0f};
    case PicamParameter_SensorTemperatureReading:
        return M{QStringLiteral("Sensor Temperature"),
                 QStringLiteral("Current sensor temperature in Celsius"),
                 QVariant(0.0), 800.0f};
    case PicamParameter_SensorTemperatureSetPoint:
        return M{QStringLiteral("Temperature Setpoint"),
                 QStringLiteral("Target sensor temperature in Celsius"),
                 QVariant(-75.0), 810.0f};
    case PicamParameter_SensorTemperatureStatus:
        return M{QStringLiteral("Temperature Status"),
                 QStringLiteral("Cooler status"),
                 QVariant(), 820.0f};
    case PicamParameter_SensorActiveWidth:
        return M{QStringLiteral("Sensor Width"),
                 QStringLiteral("Sensor width in pixels"),
                 QVariant(0), 50.0f};
    case PicamParameter_SensorActiveHeight:
        return M{QStringLiteral("Sensor Height"),
                 QStringLiteral("Sensor height in pixels"),
                 QVariant(0), 51.0f};
    case PicamParameter_SensorActiveExtendedHeight:
        return M{QStringLiteral("Sensor Extended Height"),
                 QStringLiteral("Sensor active extended height in pixels"),
                 QVariant(0), 52.0f};
    case PicamParameter_SensorSecondaryActiveHeight:
        return M{QStringLiteral("Sensor Secondary Height"),
                 QStringLiteral("Sensor secondary active height in pixels"),
                 QVariant(0), 53.0f};
    case PicamParameter_SensorActiveLeftMargin:
        return M{QStringLiteral("Sensor Left Margin"),
                 QStringLiteral("Sensor active left margin in pixels"),
                 QVariant(0), 54.0f};
    case PicamParameter_SensorActiveRightMargin:
        return M{QStringLiteral("Sensor Right Margin"),
                 QStringLiteral("Sensor active right margin in pixels"),
                 QVariant(0), 55.0f};
    case PicamParameter_SensorActiveTopMargin:
        return M{QStringLiteral("Sensor Top Margin"),
                 QStringLiteral("Sensor active top margin in pixels"),
                 QVariant(0), 56.0f};
    case PicamParameter_SensorActiveBottomMargin:
        return M{QStringLiteral("Sensor Bottom Margin"),
                 QStringLiteral("Sensor active bottom margin in pixels"),
                 QVariant(0), 57.0f};
    case PicamParameter_SensorMaskedHeight:
        return M{QStringLiteral("Sensor Masked Height"),
                 QStringLiteral("Sensor masked height in pixels"),
                 QVariant(0), 58.0f};
    case PicamParameter_SensorMaskedTopMargin:
        return M{QStringLiteral("Sensor Masked Top"),
                 QStringLiteral("Sensor masked top margin in pixels"),
                 QVariant(0), 59.0f};
    case PicamParameter_SensorMaskedBottomMargin:
        return M{QStringLiteral("Sensor Masked Bottom"),
                 QStringLiteral("Sensor masked bottom margin in pixels"),
                 QVariant(0), 60.0f};
    case PicamParameter_SensorSecondaryMaskedHeight:
        return M{QStringLiteral("Sensor Secondary Masked Height"),
                 QStringLiteral("Sensor secondary masked height in pixels"),
                 QVariant(0), 61.0f};
    case PicamParameter_SensorType:
        return M{QStringLiteral("Sensor Type"),
                 QStringLiteral("Camera sensor type"),
                 QVariant(), 62.0f};
    case PicamParameter_CcdCharacteristics:
        return M{QStringLiteral("CCD Characteristics"),
                 QStringLiteral("CCD sensor characteristics"),
                 QVariant(), 63.0f};
    case PicamParameter_Orientation:
        return M{QStringLiteral("Orientation"),
                 QStringLiteral("Physical sensor orientation"),
                 QVariant(), 64.0f};
    case PicamParameter_ReadoutOrientation:
        return M{QStringLiteral("Readout Orientation"),
                 QStringLiteral("Readout port orientation"),
                 QVariant(), 65.0f};
    case PicamParameter_PixelWidth:
        return M{QStringLiteral("Pixel Width"),
                 QStringLiteral("Pixel width in microns"),
                 QVariant(0.0), 66.0f};
    case PicamParameter_PixelHeight:
        return M{QStringLiteral("Pixel Height"),
                 QStringLiteral("Pixel height in microns"),
                 QVariant(0.0), 67.0f};
    case PicamParameter_ReadoutControlMode:
        return M{QStringLiteral("Readout Mode"),
                 QStringLiteral("Camera readout mode"),
                 QVariant(), 800.0f};
    case PicamParameter_TriggerResponse:
        return M{QStringLiteral("Trigger Response"),
                 QStringLiteral("Trigger response mode"),
                 QVariant(), 900.0f};
    case PicamParameter_TriggerDetermination:
        return M{QStringLiteral("Trigger Determination"),
                 QStringLiteral("Trigger signal polarity"),
                 QVariant(), 910.0f};
    case PicamParameter_OutputSignal:
        return M{QStringLiteral("Output Signal"),
                 QStringLiteral("Output signal selection"),
                 QVariant(), 920.0f};
    case PicamParameter_ShutterTimingMode:
        return M{QStringLiteral("Shutter Mode"),
                 QStringLiteral("Shutter timing mode"),
                 QVariant(), 1010.0f};
    case PicamParameter_ShutterClosingDelay:
        return M{QStringLiteral("Shutter Delay"),
                 QStringLiteral("Shutter closing delay in ms"),
                 QVariant(0.0), 1020.0f};
    case PicamParameter_VerticalShiftRate:
        return M{QStringLiteral("Vertical Shift Rate"),
                 QStringLiteral("Vertical shift speed in us/row"),
                 QVariant(0.0), 1030.0f};
    case PicamParameter_ActiveWidth:
        return M{QStringLiteral("Active Width"),
                 QStringLiteral("Sensor readout active width in pixels"),
                 QVariant(0), 1040.0f};
    case PicamParameter_ActiveHeight:
        return M{QStringLiteral("Active Height"),
                 QStringLiteral("Sensor readout active height in pixels"),
                 QVariant(0), 1050.0f};
    case PicamParameter_ActiveLeftMargin:
        return M{QStringLiteral("Active Left"),
                 QStringLiteral("Active area left margin in pixels"),
                 QVariant(0), 1060.0f};
    case PicamParameter_ActiveRightMargin:
        return M{QStringLiteral("Active Right"),
                 QStringLiteral("Active area right margin in pixels"),
                 QVariant(0), 1070.0f};
    case PicamParameter_ActiveTopMargin:
        return M{QStringLiteral("Active Top"),
                 QStringLiteral("Active area top margin in pixels"),
                 QVariant(0), 1080.0f};
    case PicamParameter_ActiveBottomMargin:
        return M{QStringLiteral("Active Bottom"),
                 QStringLiteral("Active area bottom margin in pixels"),
                 QVariant(0), 1090.0f};
    default:
        return M{QString(), QString(), QVariant(), 10000000.0f};
    }
}

}

//==============================================================================
// Static Members
//==============================================================================

std::atomic<bool> PicamDriver::s_sdkInitialized{false};
std::atomic<int>  PicamDriver::s_sdkRefCount{0};

//==============================================================================
// Construction / Destruction
//==============================================================================

PicamDriver::PicamDriver(QObject *parent)
    : ICameraDriver(parent)
{
}

PicamDriver::~PicamDriver()
{
    if (m_state.load() != CameraState::Disconnected) {
        disconnectCamera();
    }
}

//==============================================================================
// SDK Lifecycle (Static)
//==============================================================================

void PicamDriver::initializeSDK()
{
    if (!s_sdkInitialized.load()) {
        PicamError err = Picam_InitializeLibrary();
        if (err != PicamError_None) {
            qCCritical(driverCategory) << "Picam_InitializeLibrary failed:" << err;
            return;
        }
        s_sdkInitialized.store(true);
    }
    s_sdkRefCount.fetch_add(1);
}

void PicamDriver::shutdownSDK()
{
    if (s_sdkRefCount.fetch_sub(1) == 1) {
        PicamError err = Picam_UninitializeLibrary();
        if (err != PicamError_None) {
            qCCritical(driverCategory) << "Picam_UninitializeLibrary failed:" << err;
        }
        s_sdkInitialized.store(false);
    }
}

//==============================================================================
// Discovery
//==============================================================================

QStringList PicamDriver::enumerate()
{
    QMutexLocker locker(&m_mutex);
    initializeSDK();

    QStringList cameras;

    const PicamHandle* handles = nullptr;
    piint count = 0;
    PicamError err = Picam_GetOpenCameras(&handles, &count);

    if (err == PicamError_None && handles != nullptr && count > 0) {
        for (piint i = 0; i < count; ++i) {
            PicamCameraID id;
            if (Picam_GetCameraID(handles[i], &id) == PicamError_None) {
                const pichar* modelStr = nullptr;
                if (Picam_GetEnumerationString(PicamEnumeratedType_Model, id.model, &modelStr) == PicamError_None) {
                    QString model(modelStr);
                    Picam_DestroyString(modelStr);
                    QString serial = QString::fromLatin1(id.serial_number);
                    QString cameraId = QString("%1:%2").arg(model).arg(serial);
                    cameras.append(cameraId);
                }
            }
        }
        Picam_DestroyHandles(handles);
    }

    if (cameras.isEmpty()) {
#ifdef EZSPECCAM_PICAM_DEMO
        cameras.append("Pixis100B:123456");
#endif
    }

    shutdownSDK();
    return cameras;
}

//==============================================================================
// Connection
//==============================================================================

bool PicamDriver::connectToCamera(const QString &cameraId)
{
    QMutexLocker locker(&m_mutex);

    if (m_state.load() == CameraState::Connected) {
        disconnectCamera();
    }

    m_state.store(CameraState::Connecting);

    initializeSDK();

    // Try to find the requested camera in the list of open cameras
    const PicamHandle* handles = nullptr;
    piint count = 0;
    PicamError err = Picam_GetOpenCameras(&handles, &count);

    m_handle = nullptr;

    if (err == PicamError_None && handles != nullptr && count > 0) {
        // Search for the requested camera ID
        for (piint i = 0; i < count; ++i) {
            PicamCameraID id;
            if (Picam_GetCameraID(handles[i], &id) == PicamError_None) {
                const pichar* modelStr = nullptr;
                QString foundId;
                if (Picam_GetEnumerationString(PicamEnumeratedType_Model, id.model, &modelStr) == PicamError_None) {
                    foundId = QString("%1:%2").arg(modelStr).arg(id.serial_number);
                    Picam_DestroyString(modelStr);
                }

                if (foundId == cameraId) {
                    // Found the requested camera
                    err = Picam_OpenCamera(&id, &m_handle);
                    if (err == PicamError_None) {
                        m_connectedCameraId = foundId;
                    }
                    break;
                }
            }
        }

        Picam_DestroyHandles(handles);
    }

#ifdef EZSPECCAM_PICAM_DEMO
    if (m_handle == nullptr && cameraId == QStringLiteral("Pixis100B:123456")) {
        PicamCameraID demoId;
        PicamError demoErr = Picam_ConnectDemoCamera(PicamModel_Pixis100B, "123456", &demoId);
        if (demoErr == PicamError_None) {
            if (Picam_OpenCamera(&demoId, &m_handle) == PicamError_None) {
                m_connectedCameraId = QStringLiteral("Pixis100B:123456");
            }
        }
    }
#endif

    if (m_handle == nullptr) {
        m_lastError = CameraError::makeError(
            CameraError::Code::ConnectionFailed,
            QString("Camera not found: %1").arg(cameraId));
        m_state.store(CameraState::Disconnected);
        shutdownSDK();
        emit errorOccurred(m_lastError);
        return false;
    }

    m_parameterDefinitions.clear();
    m_parameters.clear();
    m_pendingParameters.clear();
    m_paramEnumMap.clear();
    m_paramTypeMap.clear();

    initializeParameterDefinitions();
    syncAllValuesFromHardware();

    m_lastError = CameraError();
    m_state.store(CameraState::Connected);
    emit connectionChanged(true, m_connectedCameraId);
    return true;
}

void PicamDriver::disconnectCamera()
{
    QMutexLocker locker(&m_mutex);

    if (m_state.load() == CameraState::Disconnected) {
        return;
    }

    if (m_capturing.load()) {
        stopCapture(5000);
    }

    m_state.store(CameraState::Disconnected);

    if (m_handle != nullptr) {
        Picam_CloseCamera(m_handle);
        m_handle = nullptr;
    }

    m_parameterDefinitions.clear();
    m_parameters.clear();
    m_pendingParameters.clear();
    m_paramEnumMap.clear();
    m_paramTypeMap.clear();

    shutdownSDK();
    emit connectionChanged(false, QString());
}

bool PicamDriver::isConnected() const
{
    return m_state.load() == CameraState::Connected
        || m_state.load() == CameraState::Acquiring;
}

//==============================================================================
// Parameters
//==============================================================================

QStringList PicamDriver::parameterNames() const
{
    QMutexLocker locker(&m_mutex);
    return m_parameterDefinitions.keys();
}

ParameterDefinition PicamDriver::parameter(const QString &name) const
{
    QMutexLocker locker(&m_mutex);
    if (m_parameterDefinitions.contains(name)) {
        return m_parameterDefinitions.value(name);
    }
    return ParameterDefinition();
}

QVariant PicamDriver::parameterValue(const QString &name) const
{
    QMutexLocker locker(&m_mutex);

    if (isRoiSubParam(name)) {
        return getRoiSubValue(name);
    }

    if (m_parameters.contains(name)) {
        return m_parameters.value(name);
    }
    return QVariant();
}

bool PicamDriver::setParameter(const QString &name, const QVariant &value)
{
    QMutexLocker locker(&m_mutex);

    if (!m_parameterDefinitions.contains(name)) {
        m_lastError = CameraError::makeError(
            CameraError::Code::InvalidParameter,
            QString("Unknown parameter: %1").arg(name));
        return false;
    }

    const ParameterDefinition &def = m_parameterDefinitions.value(name);

    if (def.isReadOnly) {
        return true; // Silently accept read-only params
    }

    if (!validate(value, def.constraint, def.type)) {
        m_lastError = CameraError::makeError(
            CameraError::Code::InvalidParameter,
            QString("Invalid value for %1").arg(name));
        return false;
    }

    // For ROI sub-params, cache in local ROI state
    if (isRoiSubParam(name)) {
        setRoiSubValue(name, value);
        m_roiDirty = true;
    }

    m_pendingParameters.insert(name, value);
    return true;
}

bool PicamDriver::validateParameters()
{
    QMutexLocker locker(&m_mutex);

    for (auto it = m_pendingParameters.constBegin(); it != m_pendingParameters.constEnd(); ++it) {
        const QString &name = it.key();
        const QVariant &value = it.value();

        if (!m_parameterDefinitions.contains(name)) {
            return false;
        }

        const ParameterDefinition &def = m_parameterDefinitions.value(name);

        if (!def.isReadOnly && !validate(value, def.constraint, def.type)) {
            return false;
        }
    }
    return true;
}

bool PicamDriver::commitParameters()
{
    QMutexLocker locker(&m_mutex);

    if (m_handle == nullptr) {
        return false;
    }

    if (m_roiDirty) {
        PicamRois rois;
        rois.roi_count = 1;
        rois.roi_array = &m_cachedRoi;

        PicamError err = Picam_SetParameterRoisValue(
            m_handle, PicamParameter_Rois, &rois);
        if (err != PicamError_None) {
        m_lastError = CameraError::makeError(
            CameraError::Code::CommitFailed,
            QString("Failed to set ROI: %1").arg(err));
            return false;
        }
        m_roiDirty = false;
    }

    for (auto it = m_pendingParameters.constBegin(); it != m_pendingParameters.constEnd(); ++it) {
        const QString& name = it.key();

        if (isRoiSubParam(name)) {
            continue;
        }

        if (!m_paramEnumMap.contains(name)) {
            continue;
        }

        PicamParameter picamParam = m_paramEnumMap.value(name);
        PicamValueType vt = m_paramTypeMap.value(name, PicamValueType_Integer);
        QVariant value = it.value();

        PicamError err = PicamError_UnexpectedError;

        switch (vt) {
        case PicamValueType_Integer:
            err = Picam_SetParameterIntegerValue(m_handle, picamParam, value.toInt());
            break;
        case PicamValueType_Boolean:
            err = Picam_SetParameterIntegerValue(m_handle, picamParam, value.toBool() ? 1 : 0);
            break;
        case PicamValueType_FloatingPoint:
            err = Picam_SetParameterFloatingPointValue(m_handle, picamParam, value.toDouble());
            break;
        case PicamValueType_LargeInteger:
            err = Picam_SetParameterLargeIntegerValue(m_handle, picamParam, value.toLongLong());
            break;
        case PicamValueType_Enumeration:
            err = setEnumeratedParameter(picamParam, value.toString());
            break;
        default:
            break;
        }

        if (err != PicamError_None) {
            qCWarning(parameterCategory) << "Failed to set" << name << ":" << err;
        }
    }

    const PicamParameter* failedParams = nullptr;
    piint failedCount = 0;
    PicamError err = Picam_CommitParameters(m_handle, &failedParams, &failedCount);

    if (failedParams != nullptr) {
        Picam_DestroyParameters(failedParams);
    }

    if (err != PicamError_None) {
        m_lastError = CameraError::makeError(
            CameraError::Code::CommitFailed,
            QString("Commit failed: %1 (%2 params failed)").arg(err).arg(failedCount));
        return false;
    }

    for (auto it = m_pendingParameters.constBegin(); it != m_pendingParameters.constEnd(); ++it) {
        m_parameters.insert(it.key(), it.value());
    }
    m_pendingParameters.clear();

    syncAllValuesFromHardware();

    return true;
}

//==============================================================================
// Capture
//==============================================================================

bool PicamDriver::startCapture(int captureCount)
{
    QMutexLocker locker(&m_mutex);

    if (m_state.load() != CameraState::Connected) {
        m_lastError = CameraError::makeError(
            CameraError::Code::StateInvalid,
            "Camera not connected");
        return false;
    }

    if (m_capturing.load()) {
        stopCapture(5000);
    }

    m_capturing.store(true);
    m_captureCountTarget.store(captureCount > 0 ? captureCount : 0);
    m_framesCaptured.store(0);

    m_captureThread = QThread::create([this]() {
        onCaptureLoop();
    });
    m_captureThread->start();

    m_state.store(CameraState::Acquiring);
    emit captureStarted(m_connectedCameraId);
    return true;
}

void PicamDriver::stopCapture(int timeoutMs)
{
    QMutexLocker locker(&m_mutex);

    if (!m_capturing.load()) {
        return;
    }

    m_capturing.store(false);

    if (m_captureThread != nullptr) {
        locker.unlock();
        m_captureThread->wait(timeoutMs > 0 ? timeoutMs : 5000);
        if (m_captureThread->isRunning()) {
            m_captureThread->terminate();
        }
        m_captureThread->deleteLater();
        m_captureThread = nullptr;
        locker.relock();
    }

    m_state.store(CameraState::Connected);
    emit captureStopped(m_connectedCameraId);
}

void PicamDriver::onCaptureLoop()
{
    // Use poll-style acquisition: StartAcquisition + WaitForAcquisitionUpdate loop
    // This emits frames as they arrive, rather than waiting for all frames at once

    PicamError err = Picam_StartAcquisition(m_handle);
    if (err != PicamError_None) {
        qCWarning(captureCategory) << "Picam_StartAcquisition failed:" << err;
        QMetaObject::invokeMethod(this, [this]() {
            m_capturing.store(false);
            m_state.store(CameraState::Connected);
            emit captureStopped(m_connectedCameraId);
        }, Qt::QueuedConnection);
        return;
    }

    const int pollTimeout = 1000; // 1 second polling interval
    PicamAvailableData data;
    PicamAcquisitionStatus status;

    while (m_capturing.load()) {
        int target = m_captureCountTarget.load();

        // WaitForAcquisitionUpdate blocks until data arrives or timeout
        PicamError err = Picam_WaitForAcquisitionUpdate(m_handle, pollTimeout, &data, &status);

        if (err == PicamError_TimeOutOccurred) {
            // Timeout is normal - no new data yet, continue polling
            if (target > 0 && m_framesCaptured.load() >= target) {
                break;
            }
            continue;
        }

        if (err == PicamError_AcquisitionNotInProgress) {
            // Acquisition stopped - this is normal when target frames are reached
            break;
        }

        if (err != PicamError_None) {
            qCWarning(captureCategory) << "Picam_WaitForAcquisitionUpdate failed:" << err;
            break;
        }

        if (status.errors != PicamAcquisitionErrorsMask_None) {
            qCWarning(captureCategory) << "Acquisition errors:" << status.errors;
        }

        if (data.readout_count > 0) {
            processFrame(data);
        }

        if (target > 0 && m_framesCaptured.load() >= target) {
            break;
        }
    }

    Picam_StopAcquisition(m_handle);

    QMetaObject::invokeMethod(this, [this]() {
        m_capturing.store(false);
        m_state.store(CameraState::Connected);
        emit captureStopped(m_connectedCameraId);
    }, Qt::QueuedConnection);
}

void PicamDriver::processFrame(const PicamAvailableData& data)
{
    if (data.initial_readout == nullptr || data.readout_count <= 0) {
        return;
    }

    piint frameWidth = 0;
    piint frameHeight = 0;
    Picam_GetParameterIntegerValue(m_handle, PicamParameter_SensorActiveWidth, &frameWidth);
    Picam_GetParameterIntegerValue(m_handle, PicamParameter_SensorActiveHeight, &frameHeight);

    PicamPixelFormat format;
    Picam_GetParameterIntegerValue(m_handle, PicamParameter_PixelFormat, reinterpret_cast<piint*>(&format));

    piint readoutStride = 0;
    Picam_GetParameterIntegerValue(m_handle, PicamParameter_ReadoutStride, &readoutStride);

    // Determine bytes per pixel based on PICAM format
    int bytesPerPixel = 2; // default for Monochrome16Bit
    if (format == PicamPixelFormat_Monochrome32Bit) {
        bytesPerPixel = 4;
    }

    // Calculate expected stride (width * bytesPerPixel) - PICAM may include padding
    int expectedStride = static_cast<int>(frameWidth) * bytesPerPixel;

    for (piint i = 0; i < data.readout_count; ++i) {
        const pibyte* frameData = static_cast<const pibyte*>(data.initial_readout) + (i * readoutStride);

        // Create image with calculated stride to ensure proper row alignment
        // Use Format_Grayscale8 first as safer option, convert if needed
        QImage image(static_cast<const uchar*>(frameData),
                     static_cast<int>(frameWidth),
                     static_cast<int>(frameHeight),
                     expectedStride,
                     QImage::Format_Grayscale16);

        if (image.isNull()) {
            qCWarning(captureCategory) << "Failed to create QImage from frame data, trying Format_Grayscale8";
            // Fall back to 8-bit if 16-bit fails
            QImage image8(static_cast<const uchar*>(frameData),
                          static_cast<int>(frameWidth),
                          static_cast<int>(frameHeight),
                          static_cast<int>(frameWidth),
                          QImage::Format_Grayscale8);
            if (image8.isNull()) {
                qCWarning(captureCategory) << "Failed to create 8-bit grayscale image either";
                continue;
            }
            int frameNum = m_framesCaptured.fetch_add(1);
            quint64 timestamp = QDateTime::currentMSecsSinceEpoch();
            emit frameReady(QSharedPointer<QImage>(new QImage(image8.copy())),
                           timestamp, frameNum, m_connectedCameraId, QVariantMap());
            continue;
        }

        // Copy the image to ensure it owns its data
        QImage imageCopy = image.copy();

        int frameNum = m_framesCaptured.fetch_add(1);
        quint64 timestamp = QDateTime::currentMSecsSinceEpoch();

        emit frameReady(QSharedPointer<QImage>(new QImage(std::move(imageCopy))),
                       timestamp,
                       frameNum,
                       m_connectedCameraId,
                       QVariantMap());
    }
}

//==============================================================================
// Status
//==============================================================================

CameraState PicamDriver::state() const
{
    return m_state.load();
}

QString PicamDriver::driverVersion() const
{
    return m_driverVersion;
}

QString PicamDriver::cameraId() const
{
    QMutexLocker locker(&m_mutex);
    return m_connectedCameraId;
}

//==============================================================================
// Parameter Initialization
//==============================================================================

void PicamDriver::initializeParameterDefinitions()
{
    if (m_handle == nullptr) {
        return;
    }

    const PicamParameter* params = nullptr;
    piint count = 0;
    PicamError err = Picam_GetParameters(m_handle, &params, &count);

    if (err != PicamError_None || params == nullptr) {
        qCCritical(parameterCategory) << "Picam_GetParameters failed:" << err;
        return;
    }

    for (piint i = 0; i < count; ++i) {
        PicamParameter param = params[i];

        pibln relevant = 0;
        if (Picam_IsParameterRelevant(m_handle, param, &relevant) != PicamError_None || !relevant) {
            continue;
        }

        if (param == PicamParameter_Rois) {
            initializeRoisSubParameters();
            continue;
        }

        ParameterDefinition def = buildParameterDefinition(param);
        if (!def.name.isEmpty()) {
            m_parameterDefinitions.insert(def.name, def);
            m_paramEnumMap.insert(def.name, param);
            PicamValueType vt;
            Picam_GetParameterValueType(m_handle, param, &vt);
            m_paramTypeMap.insert(def.name, vt);
        }
    }

    Picam_DestroyParameters(params);
}

ParameterDefinition PicamDriver::buildParameterDefinition(PicamParameter param)
{
    ParameterDefinition def;
    def.name = mapParameterName(param);
    if (def.name.isEmpty()) {
        return def;
    }

    auto meta = picamParameterMetadata(param);
    def.displayName = meta.displayName;
    def.description = meta.description;
    def.order = meta.order;

    def.category = categorizeParameter(param);

    PicamValueType vt;
    if (Picam_GetParameterValueType(m_handle, param, &vt) != PicamError_None) {
        return def;
    }
    def.type = mapValueType(vt);

    PicamValueAccess access;
    if (Picam_GetParameterValueAccess(m_handle, param, &access) == PicamError_None) {
        def.isReadOnly = (access == PicamValueAccess_ReadOnly);
    }

    if (vt == PicamValueType_Integer || vt == PicamValueType_LargeInteger) {
        const PicamRangeConstraint* range = nullptr;
        if (Picam_GetParameterRangeConstraint(m_handle, param, PicamConstraintCategory_Capable, &range) == PicamError_None && range != nullptr) {
            def.constraint.minValue = range->minimum;
            def.constraint.maxValue = range->maximum;
            def.constraint.step = range->increment;
        }
    } else if (vt == PicamValueType_FloatingPoint) {
        const PicamRangeConstraint* range = nullptr;
        if (Picam_GetParameterRangeConstraint(m_handle, param, PicamConstraintCategory_Capable, &range) == PicamError_None && range != nullptr) {
            def.constraint.minValue = range->minimum;
            def.constraint.maxValue = range->maximum;
            def.constraint.step = range->increment;
        }
    } else if (vt == PicamValueType_Enumeration) {
        const PicamCollectionConstraint* constraint = nullptr;
        if (Picam_GetParameterCollectionConstraint(m_handle, param, PicamConstraintCategory_Capable, &constraint) == PicamError_None && constraint != nullptr) {
            PicamEnumeratedType etype;
            Picam_GetParameterEnumeratedType(m_handle, param, &etype);
            for (piint i = 0; i < constraint->values_count; ++i) {
                const pichar* str = nullptr;
                if (Picam_GetEnumerationString(etype, static_cast<piint>(constraint->values_array[i]), &str) == PicamError_None && str != nullptr) {
                    def.constraint.validValues.append(QString(str));
                    Picam_DestroyString(str);
                }
            }
            Picam_DestroyCollectionConstraints(constraint);
        }
    }

    switch (vt) {
        case PicamValueType_Integer:
        case PicamValueType_Boolean: {
            piint v = 0;
            if (Picam_GetParameterIntegerDefaultValue(m_handle, param, &v) == PicamError_None) {
                def.defaultValue = static_cast<int>(v);
            }
            break;
        }
        case PicamValueType_Enumeration: {
            piint v = 0;
            if (Picam_GetParameterIntegerDefaultValue(m_handle, param, &v) == PicamError_None) {
                PicamEnumeratedType etype;
                if (Picam_GetParameterEnumeratedType(m_handle, param, &etype) == PicamError_None) {
                    const pichar* str = nullptr;
                    if (Picam_GetEnumerationString(etype, v, &str) == PicamError_None && str != nullptr) {
                        def.defaultValue = QString(str);
                        Picam_DestroyString(str);
                    }
                }
            }
            break;
        }
        case PicamValueType_FloatingPoint: {
            piflt v = 0.0;
            if (Picam_GetParameterFloatingPointDefaultValue(m_handle, param, &v) == PicamError_None) {
                def.defaultValue = static_cast<double>(v);
            }
            break;
        }
        case PicamValueType_LargeInteger: {
            pi64s v = 0;
            if (Picam_GetParameterLargeIntegerDefaultValue(m_handle, param, &v) == PicamError_None) {
                def.defaultValue = static_cast<qlonglong>(v);
            }
            break;
        }
        default:
            break;
    }

    if (!def.defaultValue.isValid() && meta.defaultValue.isValid()) {
        def.defaultValue = meta.defaultValue;
    }

    if (!def.defaultValue.isValid()
        && vt == PicamValueType_Enumeration
        && !def.constraint.validValues.isEmpty()) {
        def.defaultValue = def.constraint.validValues.first();
    }

    if (!def.defaultValue.isValid() && def.isReadOnly) {
        switch (vt) {
            case PicamValueType_Integer:
            case PicamValueType_Boolean: {
                piint v;
                if (Picam_GetParameterIntegerValue(m_handle, param, &v) == PicamError_None) {
                    def.defaultValue = static_cast<int>(v);
                }
                break;
            }
            case PicamValueType_Enumeration: {
                piint v;
                if (Picam_GetParameterIntegerValue(m_handle, param, &v) == PicamError_None) {
                    PicamEnumeratedType etype;
                    if (Picam_GetParameterEnumeratedType(m_handle, param, &etype) == PicamError_None) {
                        const pichar* str = nullptr;
                        if (Picam_GetEnumerationString(etype, v, &str) == PicamError_None && str != nullptr) {
                            def.defaultValue = QString(str);
                            Picam_DestroyString(str);
                        }
                    }
                }
                break;
            }
            case PicamValueType_FloatingPoint: {
                piflt v;
                if (Picam_GetParameterFloatingPointValue(m_handle, param, &v) == PicamError_None) {
                    def.defaultValue = static_cast<double>(v);
                }
                break;
            }
            case PicamValueType_LargeInteger: {
                pi64s v;
                if (Picam_GetParameterLargeIntegerValue(m_handle, param, &v) == PicamError_None) {
                    def.defaultValue = static_cast<qlonglong>(v);
                }
                break;
            }
            default:
                break;
        }
    }

    if (!def.defaultValue.isValid()) {
        DRIVER_DEBUG << "No default value for " << def.name;
        return ParameterDefinition();
    }

    DRIVER_DEBUG << "Default value for" << def.name << "is" << def.defaultValue;

    return def;
}

ParameterCategory PicamDriver::categorizeParameter(PicamParameter param) const
{
    switch (param) {
    case PicamParameter_SensorActiveWidth:
    case PicamParameter_SensorActiveHeight:
    case PicamParameter_SensorActiveExtendedHeight:
    case PicamParameter_SensorActiveLeftMargin:
    case PicamParameter_SensorActiveRightMargin:
    case PicamParameter_SensorActiveTopMargin:
    case PicamParameter_SensorActiveBottomMargin:
    case PicamParameter_SensorSecondaryActiveHeight:
    case PicamParameter_SensorMaskedHeight:
    case PicamParameter_SensorSecondaryMaskedHeight:
    case PicamParameter_SensorMaskedTopMargin:
    case PicamParameter_SensorMaskedBottomMargin:
    case PicamParameter_SensorType:
    case PicamParameter_CcdCharacteristics:
    case PicamParameter_Orientation:
    case PicamParameter_ReadoutOrientation:
    case PicamParameter_PixelWidth:
    case PicamParameter_PixelHeight:
    case PicamParameter_PixelBitDepth:
        return ParameterCategory::Info;

    case PicamParameter_SensorTemperatureStatus:
        return ParameterCategory::Info;

    case PicamParameter_SensorTemperatureReading:
    case PicamParameter_SensorTemperatureSetPoint:
        return ParameterCategory::Cooling;

    case PicamParameter_ShutterTimingMode:
    case PicamParameter_ShutterClosingDelay:
    case PicamParameter_VerticalShiftRate:
    case PicamParameter_ActiveWidth:
    case PicamParameter_ActiveHeight:
    case PicamParameter_ActiveLeftMargin:
    case PicamParameter_ActiveRightMargin:
    case PicamParameter_ActiveTopMargin:
    case PicamParameter_ActiveBottomMargin:
        return ParameterCategory::Advanced;

    default:
        return ParameterCategory::Core;
    }
}

QString PicamDriver::mapParameterName(PicamParameter param) const
{
    switch (param) {
    case PicamParameter_ExposureTime: return "exposure";
    case PicamParameter_AdcAnalogGain: return "analog_gain";
    case PicamParameter_AdcSpeed: return "adc_speed";
    case PicamParameter_AdcQuality: return "adc_quality";
    case PicamParameter_AdcBitDepth: return "adc_bit_depth";
    case PicamParameter_PixelFormat: return "pixel_format";
    case PicamParameter_PixelBitDepth: return "bit_depth";
    case PicamParameter_SensorTemperatureReading: return "sensor_temperature";
    case PicamParameter_SensorTemperatureSetPoint: return "temperature_setpoint";
    case PicamParameter_SensorTemperatureStatus: return "temperature_status";
    case PicamParameter_SensorActiveWidth: return "sensor_width";
    case PicamParameter_SensorActiveHeight: return "sensor_height";
    case PicamParameter_SensorActiveExtendedHeight: return "sensor_extended_height";
    case PicamParameter_SensorActiveLeftMargin: return "sensor_left_margin";
    case PicamParameter_SensorActiveRightMargin: return "sensor_right_margin";
    case PicamParameter_SensorActiveTopMargin: return "sensor_top_margin";
    case PicamParameter_SensorActiveBottomMargin: return "sensor_bottom_margin";
    case PicamParameter_SensorSecondaryActiveHeight: return "sensor_secondary_height";
    case PicamParameter_SensorMaskedHeight: return "sensor_masked_height";
    case PicamParameter_SensorSecondaryMaskedHeight: return "sensor_secondary_masked_height";
    case PicamParameter_SensorMaskedTopMargin: return "sensor_masked_top";
    case PicamParameter_SensorMaskedBottomMargin: return "sensor_masked_bottom";
    case PicamParameter_SensorType: return "sensor_type";
    case PicamParameter_CcdCharacteristics: return "ccd_chars";
    case PicamParameter_Orientation: return "orientation";
    case PicamParameter_ReadoutOrientation: return "readout_orientation";
    case PicamParameter_PixelWidth: return "pixel_width";
    case PicamParameter_PixelHeight: return "pixel_height";
    case PicamParameter_ReadoutControlMode: return "readout_mode";
    case PicamParameter_TriggerResponse: return "trigger_response";
    case PicamParameter_TriggerDetermination: return "trigger_determination";
    case PicamParameter_OutputSignal: return "output_signal";
    case PicamParameter_ShutterTimingMode: return "shutter_mode";
    case PicamParameter_ShutterClosingDelay: return "shutter_delay";
    case PicamParameter_VerticalShiftRate: return "vertical_shift_rate";
    case PicamParameter_ActiveWidth: return "active_width";
    case PicamParameter_ActiveHeight: return "active_height";
    case PicamParameter_ActiveLeftMargin: return "active_left";
    case PicamParameter_ActiveRightMargin: return "active_right";
    case PicamParameter_ActiveTopMargin: return "active_top";
    case PicamParameter_ActiveBottomMargin: return "active_bottom";
    default: return QString();
    }
}

void PicamDriver::initializeRoisSubParameters()
{
    const PicamRoisConstraint* constraint = nullptr;
    PicamError err = Picam_GetParameterRoisConstraint(
        m_handle, PicamParameter_Rois, PicamConstraintCategory_Capable, &constraint);

    const PicamRois* defaultRois = nullptr;
    PicamError defaultErr = Picam_GetParameterRoisDefaultValue(
        m_handle, PicamParameter_Rois, &defaultRois);

    ParameterDefinition roiX, roiY, roiW, roiH, roiXB, roiYB;

    if (defaultErr == PicamError_None && defaultRois != nullptr && defaultRois->roi_count > 0) {
        const auto& droi = defaultRois->roi_array[0];
        roiX.defaultValue = droi.x;
        roiY.defaultValue = droi.y;
        roiW.defaultValue = droi.width;
        roiH.defaultValue = droi.height;
        roiXB.defaultValue = droi.x_binning;
        roiYB.defaultValue = droi.y_binning;
        Picam_DestroyRois(defaultRois);
    } else {
        roiX.defaultValue = 0;
        roiY.defaultValue = 0;
        roiXB.defaultValue = 1;
        roiYB.defaultValue = 1;
        if (err == PicamError_None && constraint != nullptr) {
            roiW.defaultValue = constraint->width_constraint.maximum;
            roiH.defaultValue = constraint->height_constraint.maximum;
        } else {
            roiW.defaultValue = 0;
            roiH.defaultValue = 0;
        }
    }

    roiX.name = "roi_x";
    roiX.displayName = QStringLiteral("ROI X");
    roiX.description = QStringLiteral("Region of interest X offset in pixels");
    roiX.type = ParameterType::IntRange;
    roiX.category = ParameterCategory::Core;
    roiX.order = 200.0f;
    if (err == PicamError_None && constraint != nullptr) {
        roiX.constraint.minValue = constraint->x_constraint.minimum;
        roiX.constraint.maxValue = constraint->x_constraint.maximum;
        roiX.constraint.step = constraint->x_constraint.increment;
    }
    m_parameterDefinitions.insert("roi_x", roiX);
    m_paramEnumMap.insert("roi_x", PicamParameter_Rois);
    m_paramTypeMap.insert("roi_x", PicamValueType_Integer);

    roiY.name = "roi_y";
    roiY.displayName = QStringLiteral("ROI Y");
    roiY.description = QStringLiteral("Region of interest Y offset in pixels");
    roiY.type = ParameterType::IntRange;
    roiY.category = ParameterCategory::Core;
    roiY.order = 201.0f;
    if (err == PicamError_None && constraint != nullptr) {
        roiY.constraint.minValue = constraint->y_constraint.minimum;
        roiY.constraint.maxValue = constraint->y_constraint.maximum;
        roiY.constraint.step = constraint->y_constraint.increment;
    }
    m_parameterDefinitions.insert("roi_y", roiY);
    m_paramEnumMap.insert("roi_y", PicamParameter_Rois);
    m_paramTypeMap.insert("roi_y", PicamValueType_Integer);

    roiW.name = "roi_width";
    roiW.displayName = QStringLiteral("ROI Width");
    roiW.description = QStringLiteral("Region of interest width in pixels");
    roiW.type = ParameterType::IntRange;
    roiW.category = ParameterCategory::Core;
    roiW.order = 202.0f;
    if (err == PicamError_None && constraint != nullptr) {
        roiW.constraint.minValue = constraint->width_constraint.minimum;
        roiW.constraint.maxValue = constraint->width_constraint.maximum;
        roiW.constraint.step = constraint->width_constraint.increment;
    }
    m_parameterDefinitions.insert("roi_width", roiW);
    m_paramEnumMap.insert("roi_width", PicamParameter_Rois);
    m_paramTypeMap.insert("roi_width", PicamValueType_Integer);

    roiH.name = "roi_height";
    roiH.displayName = QStringLiteral("ROI Height");
    roiH.description = QStringLiteral("Region of interest height in pixels");
    roiH.type = ParameterType::IntRange;
    roiH.category = ParameterCategory::Core;
    roiH.order = 203.0f;
    if (err == PicamError_None && constraint != nullptr) {
        roiH.constraint.minValue = constraint->height_constraint.minimum;
        roiH.constraint.maxValue = constraint->height_constraint.maximum;
        roiH.constraint.step = constraint->height_constraint.increment;
    }
    m_parameterDefinitions.insert("roi_height", roiH);
    m_paramEnumMap.insert("roi_height", PicamParameter_Rois);
    m_paramTypeMap.insert("roi_height", PicamValueType_Integer);

    roiXB.name = "roi_x_binning";
    roiXB.displayName = QStringLiteral("ROI X Binning");
    roiXB.description = QStringLiteral("Horizontal hardware binning factor for ROI");
    roiXB.type = ParameterType::IntRange;
    roiXB.category = ParameterCategory::Core;
    roiXB.order = 204.0f;
    roiXB.constraint.minValue = 1;
    roiXB.constraint.maxValue = 1;
    roiXB.constraint.step = 1;
    if (err == PicamError_None && constraint != nullptr && constraint->x_binning_limits_count > 0) {
        roiXB.constraint.minValue = constraint->x_binning_limits_array[0];
        roiXB.constraint.maxValue = constraint->x_binning_limits_array[constraint->x_binning_limits_count - 1];
        roiXB.constraint.step = 1;
    }
    m_parameterDefinitions.insert("roi_x_binning", roiXB);
    m_paramEnumMap.insert("roi_x_binning", PicamParameter_Rois);
    m_paramTypeMap.insert("roi_x_binning", PicamValueType_Integer);

    roiYB.name = "roi_y_binning";
    roiYB.displayName = QStringLiteral("ROI Y Binning");
    roiYB.description = QStringLiteral("Vertical hardware binning factor for ROI");
    roiYB.type = ParameterType::IntRange;
    roiYB.category = ParameterCategory::Core;
    roiYB.order = 205.0f;
    roiYB.constraint.minValue = 1;
    roiYB.constraint.maxValue = 1;
    roiYB.constraint.step = 1;
    if (err == PicamError_None && constraint != nullptr && constraint->y_binning_limits_count > 0) {
        roiYB.constraint.minValue = constraint->y_binning_limits_array[0];
        roiYB.constraint.maxValue = constraint->y_binning_limits_array[constraint->y_binning_limits_count - 1];
        roiYB.constraint.step = 1;
    }
    m_parameterDefinitions.insert("roi_y_binning", roiYB);
    m_paramEnumMap.insert("roi_y_binning", PicamParameter_Rois);
    m_paramTypeMap.insert("roi_y_binning", PicamValueType_Integer);

    if (constraint != nullptr) {
        Picam_DestroyRoisConstraints(constraint);
    }
}

PicamValueType PicamDriver::getValueType(const QString &name) const
{
    if (m_paramTypeMap.contains(name)) {
        return m_paramTypeMap.value(name);
    }
    return PicamValueType_Integer;
}

ParameterType PicamDriver::mapValueType(PicamValueType vt) const
{
    switch (vt) {
    case PicamValueType_Integer:
    case PicamValueType_LargeInteger:
        return ParameterType::IntRange;
    case PicamValueType_Boolean:
        return ParameterType::Boolean;
    case PicamValueType_FloatingPoint:
        return ParameterType::FloatRange;
    case PicamValueType_Enumeration:
        return ParameterType::StringCollection;
    default:
        return ParameterType::IntRange;
    }
}

//==============================================================================
// Hardware Sync 
//==============================================================================

void PicamDriver::syncAllValuesFromHardware()
{
    if (m_handle == nullptr) {
        return;
    }

    const PicamRois* rois = nullptr;
    PicamError err = Picam_GetParameterRoisValue(m_handle, PicamParameter_Rois, &rois);

    if (err == PicamError_None && rois != nullptr && rois->roi_count > 0) {
        const PicamRoi& firstRoi = rois->roi_array[0];
        m_cachedRoi.x = firstRoi.x;
        m_cachedRoi.y = firstRoi.y;
        m_cachedRoi.width = firstRoi.width;
        m_cachedRoi.height = firstRoi.height;
        m_cachedRoi.x_binning = firstRoi.x_binning;
        m_cachedRoi.y_binning = firstRoi.y_binning;
        Picam_DestroyRois(rois);
    } else {
        // Initialize with sensible defaults if sync fails
        // These will be overwritten by constraint max values during ROI parameter init
        m_cachedRoi.x = 0;
        m_cachedRoi.y = 0;
        m_cachedRoi.width = 0;
        m_cachedRoi.height = 0;
        m_cachedRoi.x_binning = 1;
        m_cachedRoi.y_binning = 1;
    }

    for (auto it = m_paramEnumMap.constBegin(); it != m_paramEnumMap.constEnd(); ++it) {
        const QString& name = it.key();
        PicamParameter picamParam = it.value();

        if (isRoiSubParam(name)) {
            continue;
        }

        PicamValueType vt = m_paramTypeMap.value(name, PicamValueType_Integer);
        QVariant value;

        switch (vt) {
        case PicamValueType_Integer: {
            piint v;
            if (Picam_GetParameterIntegerValue(m_handle, picamParam, &v) == PicamError_None) {
                value = QVariant(static_cast<int>(v));
            }
            break;
        }
        case PicamValueType_Boolean: {
            piint v;
            if (Picam_GetParameterIntegerValue(m_handle, picamParam, &v) == PicamError_None) {
                value = QVariant(v != 0);
            }
            break;
        }
        case PicamValueType_FloatingPoint: {
            piflt v;
            if (Picam_GetParameterFloatingPointValue(m_handle, picamParam, &v) == PicamError_None) {
                value = QVariant(static_cast<double>(v));
            }
            break;
        }
        case PicamValueType_LargeInteger: {
            pi64s v;
            if (Picam_GetParameterLargeIntegerValue(m_handle, picamParam, &v) == PicamError_None) {
                value = QVariant(static_cast<qlonglong>(v));
            }
            break;
        }
        case PicamValueType_Enumeration: {
            piint v;
            if (Picam_GetParameterIntegerValue(m_handle, picamParam, &v) == PicamError_None) {
                PicamEnumeratedType etype;
                if (Picam_GetParameterEnumeratedType(m_handle, picamParam, &etype) == PicamError_None) {
                    const pichar* str;
                    if (Picam_GetEnumerationString(etype, v, &str) == PicamError_None) {
                        value = QString(str);
                        Picam_DestroyString(str);
                    }
                }
            }
            break;
        }
        default:
            break;
        }

        if (value.isValid()) {
            m_parameters.insert(name, value);
        }
    }
}

//==============================================================================
// ROI Sub-Parameter Helpers (Stub)
//==============================================================================

bool PicamDriver::isRoiSubParam(const QString &name) const
{
    return name == "roi_x" || name == "roi_y"
        || name == "roi_width" || name == "roi_height"
        || name == "roi_x_binning" || name == "roi_y_binning";
}

QVariant PicamDriver::getRoiSubValue(const QString &name) const
{
    if (name == "roi_x")         return QVariant(static_cast<int>(m_cachedRoi.x));
    if (name == "roi_y")         return QVariant(static_cast<int>(m_cachedRoi.y));
    if (name == "roi_width")     return QVariant(static_cast<int>(m_cachedRoi.width));
    if (name == "roi_height")    return QVariant(static_cast<int>(m_cachedRoi.height));
    if (name == "roi_x_binning") return QVariant(static_cast<int>(m_cachedRoi.x_binning));
    if (name == "roi_y_binning") return QVariant(static_cast<int>(m_cachedRoi.y_binning));
    return QVariant();
}

void PicamDriver::setRoiSubValue(const QString &name, const QVariant &value)
{
    if (name == "roi_x")         m_cachedRoi.x = value.toInt();
    if (name == "roi_y")         m_cachedRoi.y = value.toInt();
    if (name == "roi_width")     m_cachedRoi.width = value.toInt();
    if (name == "roi_height")    m_cachedRoi.height = value.toInt();
    if (name == "roi_x_binning") m_cachedRoi.x_binning = value.toInt();
    if (name == "roi_y_binning") m_cachedRoi.y_binning = value.toInt();
}

void PicamDriver::assembleRois(PicamRois &rois) const
{
    rois.roi_count = 1;
    rois.roi_array = &m_cachedRoi;
}

PicamError PicamDriver::setEnumeratedParameter(PicamParameter param, const QString &value)
{
    QString name = mapParameterNameReverse(param);
    if (name.isEmpty()) {
        return PicamError_InvalidParameterValue;
    }

    const ParameterDefinition& def = m_parameterDefinitions.value(name);
    int enumIndex = def.constraint.validValues.indexOf(value);
    if (enumIndex < 0) {
        return PicamError_InvalidParameterValue;
    }

    const PicamCollectionConstraint* constraint = nullptr;
    PicamEnumeratedType etype;

    PicamError err = Picam_GetParameterCollectionConstraint(
        m_handle, param, PicamConstraintCategory_Capable, &constraint);

    if (err != PicamError_None || constraint == nullptr) {
        return err;
    }

    if (enumIndex >= 0 && enumIndex < constraint->values_count) {
        err = Picam_SetParameterIntegerValue(m_handle, param, constraint->values_array[enumIndex]);
    } else {
        err = PicamError_InvalidParameterValue;
    }

    Picam_DestroyCollectionConstraints(constraint);
    return err;
}

QString PicamDriver::mapParameterNameReverse(PicamParameter param) const
{
    switch (param) {
    case PicamParameter_ExposureTime: return "exposure";
    case PicamParameter_AdcAnalogGain: return "analog_gain";
    case PicamParameter_AdcSpeed: return "adc_speed";
    case PicamParameter_AdcQuality: return "adc_quality";
    case PicamParameter_PixelFormat: return "pixel_format";
    case PicamParameter_PixelBitDepth: return "bit_depth";
    case PicamParameter_SensorTemperatureReading: return "sensor_temperature";
    case PicamParameter_SensorTemperatureSetPoint: return "temperature_setpoint";
    case PicamParameter_SensorActiveWidth: return "sensor_width";
    case PicamParameter_SensorActiveHeight: return "sensor_height";
    default: return QString();
    }
}
