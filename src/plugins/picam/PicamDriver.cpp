#include "PicamDriver.h"
#include "PicamParameterRegistry.h"
#include "gui/DebugMacros.h"
#include "picam.h"
#include "picam_advanced.h"

#include <QThread>
#include <QDebug>
#include <QDateTime>
#include <qthread.h>

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

bool PicamDriver::initializeSDK()
{
    if (!s_sdkInitialized.load()) {
        PicamError err = Picam_InitializeLibrary();
        if (err != PicamError_None) {
            qCCritical(driverCategory) << "Picam_InitializeLibrary failed:" << err;
            return false;
        }
        s_sdkInitialized.store(true);
    }
    s_sdkRefCount.fetch_add(1);
    return true;
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
    if (!initializeSDK()) {
        m_lastError = CameraError::makeError(
            CameraError::Code::DriverError,
            "Failed to initialize PICam SDK");
        emit errorOccurred(m_lastError);
        return QStringList();
    }

    QStringList cameras;

    const PicamCameraID* camID = nullptr;
    piint count = 0;
    PicamError err = Picam_GetAvailableCameraIDs(&camID, &count);

    if (err == PicamError_None && camID != nullptr && count > 0) {
        for (piint i = 0; i < count; ++i) {
            const pichar* modelStr = nullptr;
            if (Picam_GetEnumerationString(PicamEnumeratedType_Model, camID[i].model, &modelStr) == PicamError_None) {
                QString model(modelStr);
                Picam_DestroyString(modelStr);
                QString serial = QString::fromLatin1(camID[i].serial_number);
                QString cameraId = QString("%1-%2").arg(model).arg(serial);
                cameras.append(cameraId);
            }
        }
        Picam_DestroyCameraIDs(camID);
    }

    if (cameras.isEmpty()) {
#ifdef EZSPECCAM_PICAM_DEMO
        cameras.append("Pixis100B-123456");
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

    if (!initializeSDK()) {
        m_lastError = CameraError::makeError(
            CameraError::Code::DriverError,
            "Failed to initialize PICam SDK");
        emit errorOccurred(m_lastError);
        m_state.store(CameraState::Disconnected);
        return false;
    }

    // Try to find the requested camera in the list of available cameras
    const PicamCameraID* camID = nullptr;
    piint count = 0;
    PicamError err = Picam_GetAvailableCameraIDs(&camID, &count);

    m_handle = nullptr;

    if (err == PicamError_None && camID != nullptr && count > 0) {
        // Search for the requested camera ID
        for (piint i = 0; i < count; ++i) {
            const pichar* modelStr = nullptr;
            QString foundId;
            if (Picam_GetEnumerationString(PicamEnumeratedType_Model, camID[i].model, &modelStr) == PicamError_None) {
                foundId = QString("%1-%2").arg(QString::fromLatin1(modelStr)).arg(QString::fromLatin1(camID[i].serial_number));
                Picam_DestroyString(modelStr);
            }

            if (foundId == cameraId) {
                // Found the requested camera
                err = Picam_OpenCamera(&camID[i], &m_handle);
                if (err == PicamError_None) {
                    m_connectedCameraId = foundId;
                }
                break;
            }
        }

        Picam_DestroyCameraIDs(camID);
    }

#ifdef EZSPECCAM_PICAM_DEMO
    if (m_handle == nullptr && cameraId == QStringLiteral("Pixis100B-123456")) {
        PicamCameraID demoId;
        PicamError demoErr = Picam_ConnectDemoCamera(PicamModel_Pixis100B, "123456", &demoId);
        if (demoErr == PicamError_None) {
            if (Picam_OpenCamera(&demoId, &m_handle) == PicamError_None) {
                m_connectedCameraId = QStringLiteral("Pixis100B-123456");
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

    if (m_parameterDefinitions.contains(name)) {
        const ParameterDefinition &def = m_parameterDefinitions.value(name);

        // For dynamic or extrinsic parameters, always re-query the hardware
        // to get the latest value. The cached m_parameters value is stale
        // until the next commitParameters() call, which is not acceptable
        // for values that change with environment/time (e.g. sensor_temperature).
        if (def.isDynamic || def.isExtrinsic) {
            // need commit to update the model
            const PicamParameter* failedParams = nullptr;
            piint failedCount = 0;
            Picam_CommitParameters(m_handle, &failedParams, &failedCount);
            QVariant live = readParameterValueFromHardware(name);
            if (live.isValid()) {
                return live;
            }
            // Fall through to cache if HW read failed to keep UI responsive
            return m_parameters.value(name);
        }
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
        emit errorOccurred(m_lastError);
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
        emit errorOccurred(m_lastError);
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
            emit errorOccurred(m_lastError);
            return false;
        }
        m_roiDirty = false;
    }

    bool hasPendingError = false;

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
            m_lastError = CameraError::makeError(
                CameraError::Code::CommitFailed,
                QString("Failed to set parameter %1: %2").arg(name).arg(err),
                CameraError::Severity::Warning);
            emit errorOccurred(m_lastError);
            hasPendingError = true;
        }
    }

    const PicamParameter* failedParams = nullptr;
    piint failedCount = 0;
    PicamError err = Picam_CommitParameters(m_handle, &failedParams, &failedCount);

    if (err != PicamError_None) {
        if(failedCount > 0 && failedParams != nullptr) {
            QStringList failedParamNames;
            for(int i=0; i<failedCount; ++i) {
                const PicamParameter& p = failedParams[i];
                const PicamParameterRecord* rcd = findByPicamParam(p);
                QString paramName = rcd ? rcd->displayName : QString("UnknownParam");
                DRIVER_DEBUG << "Parameter commit failed for: " << paramName << " (PICAM param: " << p << ")";
                failedParamNames.append(paramName);
            }
            m_lastError = CameraError::makeError(
                CameraError::Code::CommitFailed,
                QString("Commit failed: %1 (%2 params failed)").arg(err).arg(failedParamNames.join(", ")));
            Picam_DestroyParameters(failedParams);
        }
        emit errorOccurred(m_lastError);
        return false;
    }

    for (auto it = m_pendingParameters.constBegin(); it != m_pendingParameters.constEnd(); ++it) {
        m_parameters.insert(it.key(), it.value());
    }
    m_pendingParameters.clear();

    syncAllValuesFromHardware();

    return !hasPendingError;
}

//==============================================================================
// Capture
//==============================================================================

bool PicamDriver::startCapture(int captureCount)
{
    QMutexLocker locker(&m_mutex);

    if (m_capturing.load()) {
        stopCapture();
    }

    if (m_state.load() != CameraState::Connected) {
        m_lastError = CameraError::makeError(
            CameraError::Code::StateInvalid,
            "Camera not connected");
        emit errorOccurred(m_lastError);
        return false;
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

    if(m_capturing.load() == false) {
        return;
    }

    m_capturing.store(false);

    if (m_handle != nullptr) {
        Picam_StopAcquisition(m_handle);
    }

    if (m_captureThread != nullptr) {
        m_captureThread->quit();
        m_captureThread->wait(timeoutMs > 0 ? timeoutMs : 5000);
        m_captureThread->deleteLater();
        m_captureThread = nullptr;
    }

    m_state.store(CameraState::Connected);
    emit captureStopped(m_connectedCameraId);
}

void PicamDriver::onCaptureLoop()
{
    int target = m_captureCountTarget.load();
    #ifdef EZSPECCAM_PICAM_DEMO
    if(target==0) target=10000;
    #endif

    pibln committed;
    Picam_SetParameterLargeIntegerValue(m_handle, PicamParameter_ReadoutCount, target);
    Picam_AreParametersCommitted( m_handle, &committed );
    if( !committed )
    {
        const PicamParameter* failed_parameter_array = NULL;
        piint           failed_parameter_count = 0;
        Picam_CommitParameters( m_handle, &failed_parameter_array, &failed_parameter_count );
        if( failed_parameter_count )
        {
            Picam_DestroyParameters( failed_parameter_array );
        }
    }

    PicamError startErr = Picam_StartAcquisition(m_handle);
    if (startErr != PicamError_None) {
        QMetaObject::invokeMethod(this, [this, startErr]() {
            m_lastError = CameraError::makeError(
                CameraError::Code::CaptureFailed,
                QString("Failed to start acquisition: %1").arg(startErr));
            emit errorOccurred(m_lastError);
        }, Qt::QueuedConnection);
        QMetaObject::invokeMethod(this, [this]() {
            m_capturing.store(false);
            m_state.store(CameraState::Connected);
            emit captureStopped(m_connectedCameraId);
        }, Qt::QueuedConnection);
        return;
    }

    PicamAvailableData data;
    PicamAcquisitionStatus status;
    pibln running = (startErr == PicamError_None);

    while (running) {
        PicamError pollErr = Picam_WaitForAcquisitionUpdate(m_handle, -1, &data, &status);

        if (pollErr != PicamError_None) {
            QMetaObject::invokeMethod(this, [this, pollErr]() {
                m_lastError = CameraError::makeError(
                    CameraError::Code::CaptureFailed,
                    QString("Acquisition update failed: %1").arg(pollErr));
                emit errorOccurred(m_lastError);
            }, Qt::QueuedConnection);
            break;
        }

        running = status.running;

        if (data.readout_count > 0) {
            processFrame(data);
        }
    }

    // stopCapture();
    QMetaObject::invokeMethod(this, "onCaptureCompleted", Qt::QueuedConnection);
}

void PicamDriver::onCaptureCompleted()
{
    stopCapture();
}

void PicamDriver::processFrame(const PicamAvailableData& data)
{
    if (data.initial_readout == nullptr || data.readout_count <= 0) {
        return;
    }

    piint frameWidth = 0;
    piint frameHeight = 0;

    PicamPixelFormat format;
    Picam_GetParameterIntegerValue(m_handle, PicamParameter_PixelFormat, reinterpret_cast<piint*>(&format));

    piint readoutStride = 0;
    Picam_GetParameterIntegerValue(m_handle, PicamParameter_ReadoutStride, &readoutStride);

    // Determine bytes per pixel based on PICAM format
    int bytesPerPixel = 2; // default for Monochrome16Bit
    if (format == PicamPixelFormat_Monochrome32Bit) {
        bytesPerPixel = 4;
    }

    piint binX = m_cachedRoi.x_binning > 0 ? m_cachedRoi.x_binning : 1;
    piint binY = m_cachedRoi.y_binning > 0 ? m_cachedRoi.y_binning : 1;
    frameWidth  = m_cachedRoi.width  / binX;
    frameHeight = m_cachedRoi.height / binY;

    // Calculate expected stride (width * bytesPerPixel) - PICAM may include padding
    int expectedStride = static_cast<int>(frameWidth) * bytesPerPixel;
    piint expectedFrameSize = frameWidth * frameHeight * bytesPerPixel;
    if (readoutStride > 0 && readoutStride != expectedFrameSize) {
        qCWarning(captureCategory) << "ROI/stride mismatch: expectedFrameSize="
                                   << expectedFrameSize << " readoutStride="
                                   << readoutStride
                                   << " (m_cachedRoi may be out of sync with hardware)";
    }

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
            QMetaObject::invokeMethod(this, [this]() {
                m_lastError = CameraError::makeError(
                    CameraError::Code::CaptureFailed,
                    "Failed to create 16-bit QImage from frame data, falling back to 8-bit",
                    CameraError::Severity::Warning);
                emit errorOccurred(m_lastError);
            }, Qt::QueuedConnection);
            // Fall back to 8-bit if 16-bit fails
            QImage image8(static_cast<const uchar*>(frameData),
                          static_cast<int>(frameWidth),
                          static_cast<int>(frameHeight),
                          static_cast<int>(frameWidth),
                          QImage::Format_Grayscale8);
            if (image8.isNull()) {
                QMetaObject::invokeMethod(this, [this]() {
                    m_lastError = CameraError::makeError(
                        CameraError::Code::CaptureFailed,
                        "Failed to create 8-bit QImage from frame data",
                        CameraError::Severity::Warning);
                    emit errorOccurred(m_lastError);
                }, Qt::QueuedConnection);
                continue;
            }
            int frameNum = m_framesCaptured.fetch_add(1);
            quint64 timestamp = QDateTime::currentMSecsSinceEpoch();
            emit frameReady(QSharedPointer<QImage>(new QImage(image8.copy())),
                           timestamp, frameNum, m_connectedCameraId,
                           captureParametersSnapshot());
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
                       captureParametersSnapshot());
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
        m_lastError = CameraError::makeError(
            CameraError::Code::DriverError,
            QString("Failed to get camera parameters: %1").arg(err));
        emit errorOccurred(m_lastError);
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
    def.name = picamParamName(param);
    if (def.name.isEmpty()) {
        return def;
    }

    const PicamParameterRecord *rec = findByPicamParam(param);
    if (rec) {
        def.displayName = rec->displayName;
        def.description = rec->description;
        def.order = rec->order;
        def.category = rec->category;
    } else {
        def.category = ParameterCategory::Core;
    }

    PicamValueType vt;
    if (Picam_GetParameterValueType(m_handle, param, &vt) != PicamError_None) {
        return def;
    }
    def.type = mapValueType(vt);

    PicamValueAccess access;
    if (Picam_GetParameterValueAccess(m_handle, param, &access) == PicamError_None) {
        def.isReadOnly = (access == PicamValueAccess_ReadOnly);
    }

    PicamDynamicsMask dynamicsMask = PicamDynamicsMask_None;
    if (PicamAdvanced_GetParameterDynamics(m_handle, param, &dynamicsMask) == PicamError_None) {
        def.isDynamic = (dynamicsMask != PicamDynamicsMask_None);
    }

    PicamDynamicsMask extrinsicMask = PicamDynamicsMask_None;
    if (PicamAdvanced_GetParameterExtrinsicDynamics(m_handle, param, &extrinsicMask) == PicamError_None) {
        def.isExtrinsic = (extrinsicMask != PicamDynamicsMask_None);
    }

    bool constraintLoaded = false;

    if (vt == PicamValueType_Enumeration) {
        const PicamCollectionConstraint* constraint = nullptr;
        if (Picam_GetParameterCollectionConstraint(m_handle, param, PicamConstraintCategory_Required, &constraint) == PicamError_None && constraint != nullptr) {
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
            constraintLoaded = true;
        }
    } else {
        const PicamRangeConstraint* range = nullptr;
        if (Picam_GetParameterRangeConstraint(m_handle, param, PicamConstraintCategory_Required, &range) == PicamError_None && range != nullptr) {
            def.constraint.minValue = range->minimum;
            def.constraint.maxValue = range->maximum;
            def.constraint.step = range->increment;
            constraintLoaded = true;
        }

        if (!constraintLoaded) {
            const PicamCollectionConstraint* collection = nullptr;
            if (Picam_GetParameterCollectionConstraint(m_handle, param, PicamConstraintCategory_Required, &collection) == PicamError_None && collection != nullptr) {
                for (piint i = 0; i < collection->values_count; ++i) {
                    def.constraint.validValues.append(static_cast<double>(collection->values_array[i]));
                }
                Picam_DestroyCollectionConstraints(collection);
                constraintLoaded = true;

                if (vt == PicamValueType_FloatingPoint) {
                    def.type = ParameterType::FloatCollection;
                } else if (vt == PicamValueType_Boolean) {
                    def.type = ParameterType::Boolean;
                } else {
                    def.type = ParameterType::IntCollection;
                }
            }
        }
    }

    if (param == PicamParameter_ExposureTime) {
        def.constraint.unit = {QStringLiteral("ms"), QStringLiteral("s"), QStringLiteral("min")};
        def.constraint.unitRange = {1000.0, 60000.0};
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

    if (!def.defaultValue.isValid()
        && vt == PicamValueType_Enumeration
        && !def.constraint.validValues.isEmpty()) {
        def.defaultValue = def.constraint.validValues.first();
    }

    if (!def.defaultValue.isValid()) {
        DRIVER_DEBUG << "No default value for " << def.name;
        return ParameterDefinition();
    }

    DRIVER_DEBUG << "Default value for" << def.name << "is" << def.defaultValue;

    return def;
}

void PicamDriver::initializeRoisSubParameters()
{
    const PicamRoisConstraint* constraint = nullptr;
    PicamError err = Picam_GetParameterRoisConstraint(
        m_handle, PicamParameter_Rois, PicamConstraintCategory_Required, &constraint);

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
    roiXB.constraint.maxValue = constraint->width_constraint.maximum;
    roiXB.constraint.step = 1;
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
    roiYB.constraint.maxValue = constraint->height_constraint.maximum;
    roiYB.constraint.step = 1;
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
        if (isRoiSubParam(name)) {
            continue;
        }
        QVariant value = readParameterValueFromHardware(name);
        if (value.isValid()) {
            m_parameters.insert(name, value);
        }
    }
}

QVariantMap PicamDriver::captureParametersSnapshot() const
{
    QMutexLocker locker(&m_mutex);
    return m_parameters;
}

QVariant PicamDriver::readParameterValueFromHardware(const QString &name) const
{
    if (m_handle == nullptr) {
        return QVariant();
    }
    if (!m_paramEnumMap.contains(name)) {
        return QVariant();
    }

    PicamParameter picamParam = m_paramEnumMap.value(name);
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

    return value;
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
    QString name = picamParamName(param);
    if (name.isEmpty()) {
        return PicamError_InvalidParameterValue;
    }

    const ParameterDefinition& def = m_parameterDefinitions.value(name);
    int enumIndex = def.constraint.validValues.indexOf(value);
    if (enumIndex < 0) {
        return PicamError_InvalidParameterValue;
    }

    const PicamCollectionConstraint* constraint = nullptr;

    PicamError err = Picam_GetParameterCollectionConstraint(
        m_handle, param, PicamConstraintCategory_Required, &constraint);

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
