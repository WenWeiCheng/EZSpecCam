#include "PicamDriver.h"

#include <QThread>
#include <QDebug>

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
        // TODO: Picam_InitializeLibrary();
        s_sdkInitialized.store(true);
    }
    s_sdkRefCount.fetch_add(1);
}

void PicamDriver::shutdownSDK()
{
    if (s_sdkRefCount.fetch_sub(1) == 1) {
        // TODO: Picam_UninitializeLibrary();
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
    // TODO: Enumerate connected PI cameras via Picam_OpenFirstCamera
    // cameras << buildCameraIdString(...);
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

    // TODO: Picam_OpenFirstCamera(&m_handle) or Picam_OpenCamera(&m_cameraID, &m_handle)
    // TODO: Picam_GetCameraID(m_handle, &m_cameraID)
    m_handle = nullptr;
    m_connectedCameraId = cameraId;

    m_parameterDefinitions.clear();
    m_parameters.clear();
    m_pendingParameters.clear();
    m_paramEnumMap.clear();
    m_paramTypeMap.clear();

    // TODO: initializeParameterDefinitions();
    // TODO: syncAllValuesFromHardware();

    m_lastError = CameraError();
    m_state.store(CameraState::Connected);
    emit connectionChanged(true, cameraId);
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

    // TODO: Picam_CloseCamera(m_handle);
    m_handle = nullptr;

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

    // TODO: Apply pending parameters to camera via typed Picam setters
    // TODO: Handle ROI assembly: Picam_SetParameterRoisValue(m_handle, PicamParameter_Rois, &assembledRois)
    // TODO: Picam_CommitParameters(m_handle, &failedParams, &failedCount)

    // Move pending to active
    for (auto it = m_pendingParameters.constBegin(); it != m_pendingParameters.constEnd(); ++it) {
        m_parameters.insert(it.key(), it.value());
    }
    m_pendingParameters.clear();
    m_roiDirty = false;

    // TODO: syncAllValuesFromHardware();
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

    // TODO: Picam_Acquire() loop on dedicated thread
    m_capturing.store(true);
    m_captureCountTarget.store(captureCount);
    m_framesCaptured.store(0);
    m_state.store(CameraState::Acquiring);
    emit captureStarted(m_connectedCameraId);
    return true;
}

void PicamDriver::stopCapture(int timeoutMs)
{
    Q_UNUSED(timeoutMs);
    QMutexLocker locker(&m_mutex);

    if (!m_capturing.load()) {
        return;
    }

    m_capturing.store(false);
    m_state.store(CameraState::Connected);
    emit captureStopped(m_connectedCameraId);
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
    // TODO: Iterate Picam_GetParameters() results
    // TODO: For each PicamParameter, build a ParameterDefinition
    // TODO: Skip irrelevant/non-existent params
    // TODO: Handle PicamParameter_Rois — decompose into sub-params
    // TODO: Categorize into Core / Cooling / Info / Advanced / Debug
}

ParameterDefinition PicamDriver::buildParameterDefinition(PicamParameter param)
{
    Q_UNUSED(param);
    return ParameterDefinition();
}

ParameterCategory PicamDriver::categorizeParameter(PicamParameter param) const
{
    Q_UNUSED(param);
    return ParameterCategory::Core;
}

QString PicamDriver::mapParameterName(PicamParameter param) const
{
    Q_UNUSED(param);
    return QString();
}

PicamValueType PicamDriver::getValueType(const QString &name) const
{
    if (m_paramTypeMap.contains(name)) {
        return m_paramTypeMap.value(name);
    }
    return PicamValueType_Integer;
}

//==============================================================================
// Hardware Sync (Stub)
//==============================================================================

void PicamDriver::syncAllValuesFromHardware()
{
    // TODO: Iterate all parameters and call appropriate Picam_GetParameter*Value()
    // TODO: Cache composite PicamRois for roi_* sub-param access
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
    Q_UNUSED(rois);
    // TODO: Build PicamRois from m_cachedRoi fields
}
