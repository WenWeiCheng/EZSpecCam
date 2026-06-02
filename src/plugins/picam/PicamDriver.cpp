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
        cameras.append("Pixis100B:123456");
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
                        break;
                    }
                }
            }
        }

        // If not found, try opening the first available camera
        if (m_handle == nullptr) {
            PicamCameraID firstId;
            if (Picam_GetCameraID(handles[0], &firstId) == PicamError_None) {
                err = Picam_OpenCamera(&firstId, &m_handle);
                if (err == PicamError_None) {
                    const pichar* modelStr = nullptr;
                    if (Picam_GetEnumerationString(PicamEnumeratedType_Model, firstId.model, &modelStr) == PicamError_None) {
                        m_connectedCameraId = QString("%1:%2").arg(modelStr).arg(firstId.serial_number);
                        Picam_DestroyString(modelStr);
                    }
                }
            }
        }

        Picam_DestroyHandles(handles);
    }

    // If no camera opened yet, try demo camera
    if (m_handle == nullptr) {
        PicamCameraID demoId;
        err = Picam_ConnectDemoCamera(PicamModel_Pixis100B, "123456", &demoId);
        if (err == PicamError_None) {
            err = Picam_OpenCamera(&demoId, &m_handle);
            if (err == PicamError_None) {
                m_connectedCameraId = "Pixis100B:123456";
            }
        }
    }

    if (m_handle == nullptr) {
        m_lastError = CameraError::makeError(
            CameraError::Code::ConnectionFailed,
            QString("Failed to connect to %1").arg(cameraId));
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
// Parameter Initialization (Stub)
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

    return def;
}

ParameterCategory PicamDriver::categorizeParameter(PicamParameter param) const
{
    if (param == PicamParameter_SensorTemperatureReading ||
        param == PicamParameter_SensorTemperatureSetPoint) {
        return ParameterCategory::Cooling;
    }

    if (param == PicamParameter_SensorActiveWidth ||
        param == PicamParameter_SensorActiveHeight) {
        return ParameterCategory::Info;
    }

    return ParameterCategory::Core;
}

QString PicamDriver::mapParameterName(PicamParameter param) const
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

void PicamDriver::initializeRoisSubParameters()
{
    const PicamRoisConstraint* constraint = nullptr;
    PicamError err = Picam_GetParameterRoisConstraint(
        m_handle, PicamParameter_Rois, PicamConstraintCategory_Capable, &constraint);

    ParameterDefinition roiX, roiY, roiW, roiH, roiXB, roiYB;

    roiX.name = "roi_x";
    roiX.type = ParameterType::IntRange;
    roiX.category = ParameterCategory::Core;
    if (err == PicamError_None && constraint != nullptr) {
        roiX.constraint.minValue = constraint->x_constraint.minimum;
        roiX.constraint.maxValue = constraint->x_constraint.maximum;
        roiX.constraint.step = constraint->x_constraint.increment;
    }
    m_parameterDefinitions.insert("roi_x", roiX);
    m_paramEnumMap.insert("roi_x", PicamParameter_Rois);
    m_paramTypeMap.insert("roi_x", PicamValueType_Integer);

    roiY.name = "roi_y";
    roiY.type = ParameterType::IntRange;
    roiY.category = ParameterCategory::Core;
    if (err == PicamError_None && constraint != nullptr) {
        roiY.constraint.minValue = constraint->y_constraint.minimum;
        roiY.constraint.maxValue = constraint->y_constraint.maximum;
        roiY.constraint.step = constraint->y_constraint.increment;
    }
    m_parameterDefinitions.insert("roi_y", roiY);
    m_paramEnumMap.insert("roi_y", PicamParameter_Rois);
    m_paramTypeMap.insert("roi_y", PicamValueType_Integer);

    roiW.name = "roi_width";
    roiW.type = ParameterType::IntRange;
    roiW.category = ParameterCategory::Core;
    if (err == PicamError_None && constraint != nullptr) {
        roiW.constraint.minValue = constraint->width_constraint.minimum;
        roiW.constraint.maxValue = constraint->width_constraint.maximum;
        roiW.constraint.step = constraint->width_constraint.increment;
    }
    m_parameterDefinitions.insert("roi_width", roiW);
    m_paramEnumMap.insert("roi_width", PicamParameter_Rois);
    m_paramTypeMap.insert("roi_width", PicamValueType_Integer);

    roiH.name = "roi_height";
    roiH.type = ParameterType::IntRange;
    roiH.category = ParameterCategory::Core;
    if (err == PicamError_None && constraint != nullptr) {
        roiH.constraint.minValue = constraint->height_constraint.minimum;
        roiH.constraint.maxValue = constraint->height_constraint.maximum;
        roiH.constraint.step = constraint->height_constraint.increment;
    }
    m_parameterDefinitions.insert("roi_height", roiH);
    m_paramEnumMap.insert("roi_height", PicamParameter_Rois);
    m_paramTypeMap.insert("roi_height", PicamValueType_Integer);

    roiXB.name = "roi_x_binning";
    roiXB.type = ParameterType::IntRange;
    roiXB.category = ParameterCategory::Core;
    if (err == PicamError_None && constraint != nullptr && constraint->x_binning_limits_count > 0) {
        roiXB.constraint.minValue = constraint->x_binning_limits_array[0];
        roiXB.constraint.maxValue = constraint->x_binning_limits_array[constraint->x_binning_limits_count - 1];
        roiXB.constraint.step = 1;
    }
    m_parameterDefinitions.insert("roi_x_binning", roiXB);
    m_paramEnumMap.insert("roi_x_binning", PicamParameter_Rois);
    m_paramTypeMap.insert("roi_x_binning", PicamValueType_Integer);

    roiYB.name = "roi_y_binning";
    roiYB.type = ParameterType::IntRange;
    roiYB.category = ParameterCategory::Core;
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
// Hardware Sync (Stub)
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
