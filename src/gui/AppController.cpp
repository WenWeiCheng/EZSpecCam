#include "AppController.h"
#include "DebugMacros.h"
#include <QDebug>
#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <qobject.h>


AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_pluginDir(QCoreApplication::applicationDirPath() + "/plugins/drivers")
{
}

AppController::~AppController()
{
    if (m_driver) {
        disconnectCamera();
        saveDynamicConfig(m_cameraId, m_parameters);
    }
    clearPlugins();
}

void AppController::setPluginDirectory(const QString &path)
{
    m_pluginDir = path;
}

void AppController::scanPlugins()
{
    clearPlugins();

    QDir pluginsDir(m_pluginDir);
    if (!pluginsDir.exists()) {
        qWarning() << "AppController: Plugin directory does not exist:" << m_pluginDir;
        emit pluginScanCompleted(0, 0);
        return;
    }

    QStringList dllFilters;
    dllFilters << "*.dll";

    QFileInfoList fileList = pluginsDir.entryInfoList(dllFilters, QDir::Files);
    int totalPlugins = fileList.size();
    int loadedPlugins = 0;

    for (int i = 0; i < fileList.size(); ++i) {
        QString filePath = fileList[i].absoluteFilePath();

        bool alreadyLoaded = false;
        for (const auto &existing : m_plugins) {
            if (QFileInfo(existing.filePath).canonicalFilePath() == QFileInfo(filePath).canonicalFilePath()) {
                alreadyLoaded = true;
                break;
            }
        }
        if (alreadyLoaded) {
            loadedPlugins++;
            continue;
        }

        emit pluginScanProgress(i + 1, totalPlugins, filePath);

        if (loadPlugin(filePath)) {
            loadedPlugins++;
        }
    }

    emit pluginScanCompleted(totalPlugins, loadedPlugins);
}

QStringList AppController::availableCameras() const
{
    QStringList cameras;
    for (const PluginInfo &info : m_plugins) {
        if (!info.cameraIds.isEmpty()) {
            cameras.append(info.cameraIds);
        }
    }
    return cameras;
}

QList<PluginInfo> AppController::loadedPlugins() const
{
    return m_plugins;
}

bool AppController::hasPlugins() const
{
    return !m_plugins.isEmpty();
}

bool AppController::connectCamera(const QString &cameraId)
{
    if (!canConnect()) {
        qWarning() << "AppController: Cannot connect - current state:" << static_cast<int>(m_state);
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::StateInvalid,
            "Cannot connect: Already connected or invalid state"));
        emit connectCameraFinished(cameraId, false, QStringLiteral("Invalid state"));
        return false;
    }

    if (cameraId.isEmpty()) {
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::InvalidParameter,
            "Cannot connect: Empty camera ID"));
        emit connectCameraFinished(cameraId, false, QStringLiteral("Empty camera ID"));
        return false;
    }

    enterConnectingState();
    m_cameraId = cameraId;

    const PluginInfo *pluginInfo = findPluginForCamera(cameraId);
    if (!pluginInfo) {
        enterErrorState(CameraError::makeError(
            CameraError::Code::PluginLoadFailed,
            "No plugin found for camera: " + cameraId));
        emit connectCameraFinished(cameraId, false, QStringLiteral("No plugin found"));
        return false;
    }

    if (pluginInfo->instance) {
        m_driver = pluginInfo->instance;
    } else if (pluginInfo->loader && pluginInfo->loader->isLoaded()) {
        QObject *plugin = pluginInfo->loader->instance();
        m_driver = qobject_cast<ICameraDriver*>(plugin);
        if (!m_driver) {
            enterErrorState(CameraError::makeError(
                CameraError::Code::PluginLoadFailed,
                "Plugin does not implement ICameraDriver"));
            emit connectCameraFinished(cameraId, false, QStringLiteral("Plugin does not implement ICameraDriver"));
            return false;
        }
    } else {
        enterErrorState(CameraError::makeError(
            CameraError::Code::PluginLoadFailed,
            "Failed to get driver instance"));
        emit connectCameraFinished(cameraId, false, QStringLiteral("Failed to get driver instance"));
        return false;
    }

    connect(m_driver, &ICameraDriver::frameReady,
            this, &AppController::onDriverFrameReady, Qt::DirectConnection);
    connect(m_driver, &ICameraDriver::captureStarted,
            this, &AppController::onDriverCaptureStarted, Qt::DirectConnection);
    connect(m_driver, &ICameraDriver::captureStopped,
            this, &AppController::onDriverCaptureStopped, Qt::DirectConnection);
    connect(m_driver, &ICameraDriver::connectionChanged,
            this, &AppController::onDriverConnectionChanged, Qt::DirectConnection);
    connect(m_driver, &ICameraDriver::errorOccurred,
            this, &AppController::onDriverError, Qt::DirectConnection);

    if (!m_driver->connectToCamera(cameraId)) {
        qWarning() << "AppController: Driver failed to connect";
        enterErrorState(CameraError::makeError(
            CameraError::Code::ConnectionFailed,
            "Driver failed to connect"));
        emit connectCameraFinished(cameraId, false, QStringLiteral("Driver failed to connect"));
        return false;
    }

    enterConnectedState();
    emit connectCameraFinished(cameraId, true, QString());
    return true;
}

void AppController::disconnectCamera()
{
    if (!canDisconnect()) {
        return;
    }

    if (m_state == CameraState::Acquiring) {
        stopCapture();
    }

    disconnectFromDriver();
    enterDisconnectedState();
    
    m_fisrtSetParameter = true;
}

bool AppController::isConnected() const
{
    return m_state == CameraState::Connected;
}

bool AppController::hasError() const
{
    return m_state == CameraState::Error;
}

QString AppController::currentCameraId() const
{
    return m_cameraId;
}

bool AppController::startCapture(int captureCount)
{
    if (!canStartCapture()) {
        qWarning() << "AppController: Cannot start capture - not connected";
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::StateInvalid,
            "Cannot start capture: Camera not ready"));
        return false;
    }

    if (!m_driver) {
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::NotConnected,
            "Cannot start capture: No driver"));
        return false;
    }

    m_captureCount = captureCount;

    if (!m_driver->startCapture(captureCount)) {
        qWarning() << "AppController: Driver failed to start capture";
        emit errorOccurred(CameraError::makeError(
            CameraError::Code::CaptureFailed,
            "Failed to start capture"));
        return false;
    }

    return true;
}

void AppController::stopCapture(int timeoutMs)
{
    if (!canStopCapture()) {
        return;
    }

    if (m_driver) {
        m_driver->stopCapture(timeoutMs);
    }
}

bool AppController::canDisconnect() const
{
    return m_state != CameraState::Disconnected;
}

bool AppController::canStartCapture() const
{
    return m_state == CameraState::Connected;
}

bool AppController::canStopCapture() const
{
    return m_state == CameraState::Acquiring;
}

bool AppController::canConfigure() const
{
    return m_state == CameraState::Connected;
}

void AppController::enterConnectingState()
{
    setState(CameraState::Connecting);
}

void AppController::enterConnectedState()
{
    setState(CameraState::Connected);
}

void AppController::enterAcquiringState()
{
    setState(CameraState::Acquiring);
}

void AppController::enterDisconnectedState()
{
    setState(CameraState::Disconnected);
}

void AppController::enterErrorState(const CameraError &error)
{
    m_lastError = error;
    setState(CameraState::Error);
    emit errorOccurred(error);
}

void AppController::disconnectFromDriver()
{
    if (!m_driver) {
        return;
    }

    disconnect(m_driver, nullptr, this, nullptr);
    m_driver->disconnectCamera();
    m_driver = nullptr;
    m_cameraId.clear();
}

void AppController::cleanupDriver()
{
    if (m_driver) {
        disconnect(m_driver, nullptr, this, nullptr);
        m_driver = nullptr;
    }
    m_cameraId.clear();
}

const PluginInfo *AppController::findPluginForCamera(const QString &cameraId) const
{
    for (const PluginInfo &info : m_plugins) {
        if (info.cameraIds.contains(cameraId)) {
            return &info;
        }
    }
    return nullptr;
}

bool AppController::loadPlugin(const QString &filePath)
{
    QPluginLoader *loader = new QPluginLoader(filePath, this);

    if (!loader->load()) {
        QString error = loader->errorString();
        qWarning() << "AppController: Failed to load plugin" << filePath << "-" << error;
        emit pluginLoadFailed(filePath, error);
        loader->deleteLater();
        return false;
    }

    QObject *plugin = loader->instance();
    if (!plugin) {
        QString error = loader->errorString();
        qWarning() << "AppController: Failed to get plugin instance from" << filePath << "-" << error;
        loader->unload();
        loader->deleteLater();
        emit pluginLoadFailed(filePath, error);
        return false;
    }

    ICameraDriver *driver = qobject_cast<ICameraDriver*>(plugin);
    if (!driver) {
        qWarning() << "AppController: Plugin does not implement ICameraDriver";
        loader->unload();
        loader->deleteLater();
        emit pluginLoadFailed(filePath, "Plugin does not implement ICameraDriver");
        return false;
    }

    QStringList cameraIds = driver->enumerate();
    if (cameraIds.isEmpty()) {
        qWarning() << "AppController: Plugin returned no cameras";
    }

    PluginInfo info;
    info.filePath = filePath;
    info.cameraIds = cameraIds;
    info.loader = loader;
    info.instance = nullptr;

    m_plugins.append(info);

    qDebug() << "AppController: Loaded plugin" << filePath << "with cameras:" << cameraIds;
    return true;
}

void AppController::unloadPlugin(const PluginInfo &info)
{
    if (info.loader) {
        if (info.loader->isLoaded()) {
            info.loader->unload();
        }
        info.loader->deleteLater();
    }
}

void AppController::clearPlugins()
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ) {
        const PluginInfo &info = *it;
        if (m_driver && info.loader && info.loader->instance() == m_driver) {
            ++it;
            continue;
        }
        unloadPlugin(info);
        it = m_plugins.erase(it);
    }
}

QString AppController::getConfigDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/configs";
}

QString AppController::getConfigPath(const QString &cameraId)
{
    return getConfigDirectory() + "/" + cameraId + ".ini";
}

void AppController::saveDynamicConfig(const QString &cameraId, const QVariantMap &parameters)
{
    QString path = getConfigPath(cameraId);

    QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "AppController: Failed to create directory for:" << path;
            return;
        }
    }

    if (!saveParameters(path, cameraId, parameters)) {
        qWarning() << "AppController: Failed to save dynamic config to:" << path;
    }
}

QVariantMap AppController::loadDynamicConfig(const QString &cameraId)
{
    QVariantMap parameters;
    QString path = getConfigPath(cameraId);

    QFile file(path);
    if (!file.exists()) {
        return parameters;
    }

    QString camId;
    if (!loadParameters(path, camId, parameters)) {
        qWarning() << "AppController: Failed to load dynamic config file:" << path;
        return QVariantMap();
    }

    return parameters;
}

bool AppController::ensureDirectoryExists(const QString &dirPath)
{
    QFileInfo fileInfo(dirPath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        return dir.mkpath(".");
    }
    return true;
}

QString AppController::variantToString(const QVariant &value)
{
    QString typePrefix;
    QString stringValue;

    switch (value.typeId()) {
    case QMetaType::Double:
    case QMetaType::Float:
        typePrefix = "double:";
        stringValue = value.toString();
        break;
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::UInt:
    case QMetaType::ULongLong:
        typePrefix = "int:";
        stringValue = value.toString();
        break;
    case QMetaType::Bool:
        typePrefix = "bool:";
        stringValue = value.toString();
        break;
    default:
        typePrefix = "string:";
        stringValue = value.toString();
        break;
    }

    return typePrefix + stringValue;
}

QVariant AppController::stringToVariant(const QString &valueStr)
{
    int colonPos = valueStr.indexOf(':');
    if (colonPos <= 0) {
        return valueStr;
    }

    QString typePrefix = valueStr.left(colonPos);
    QString actualValue = valueStr.mid(colonPos + 1);

    if (typePrefix == "double") {
        return QVariant(actualValue).toDouble();
    } else if (typePrefix == "int") {
        return QVariant(actualValue).toInt();
    } else if (typePrefix == "int64") {
        return QVariant(actualValue).toLongLong();
    } else if (typePrefix == "bool") {
        return QVariant(actualValue).toBool();
    } else {
        return actualValue;
    }
}

bool AppController::saveParameters(const QString &filePath,
                                 const QString &cameraId,
                                 const QVariantMap &parameters)
{
    if (!ensureDirectoryExists(filePath)) {
        qWarning() << "AppController: Failed to create directory for" << filePath;
        return false;
    }

    QSettings settings(filePath, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) {
        qWarning() << "AppController: Failed to create QSettings for" << filePath;
        return false;
    }

    settings.beginGroup("Camera");
    settings.setValue("id", cameraId);
    settings.endGroup();

    settings.beginGroup("Parameters");
    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        settings.setValue(it.key(), variantToString(it.value()));
    }
    settings.endGroup();

    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool AppController::loadParameters(const QString &filePath,
                                 QString &cameraId,
                                 QVariantMap &parameters)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        return false;
    }

    QSettings settings(filePath, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) {
        return false;
    }

    settings.beginGroup("Camera");
    cameraId = settings.value("id").toString();
    settings.endGroup();

    parameters.clear();
    settings.beginGroup("Parameters");
    const QStringList keys = settings.allKeys();
    for (const QString &key : keys) {
        parameters.insert(key, stringToVariant(settings.value(key).toString()));
    }
    settings.endGroup();

    return true;
}

// ——— Public State Access ———

CameraState AppController::state() const
{
    return m_state;
}

// ——— Private Helpers ———

void AppController::setState(CameraState newState)
{
    if (m_state != newState) {
        m_state = newState;
        emit stateChanged(newState);
    }
}

bool AppController::canConnect() const
{
    return m_state == CameraState::Disconnected || m_state == CameraState::Error;
}

// ——— Parameter Management ———

QVariantMap AppController::allParameters() const
{
    return m_parameters;
}

QStringList AppController::parameterNames() const
{
    if (!m_driver) {
        return QStringList();
    }
    return m_driver->parameterNames();
}

ParameterDefinition AppController::parameter(const QString &name) const
{
    if (!m_driver) {
        return ParameterDefinition();
    }
    return m_driver->parameter(name);
}

QVariant AppController::parameterValue(const QString &name) const
{
    if (!m_driver) {
        return QVariant();
    }
    return m_driver->parameterValue(name);
}

bool AppController::setParameter(const QString &name, const QVariant &value)
{
    if (!m_driver) {
        return false;
    }

    QVariant oldValue = m_parameters.value(name);
    if (!m_fisrtSetParameter && oldValue == value && !m_pendingParameters.contains(name)) {
        return true;
    }

    if (m_driver->setParameter(name, value)) {
        m_pendingParameters[name] = value;
        if(m_fisrtSetParameter){
            PARAM_DEBUG << "First set parameter:" << name << ":" << value;
        } else {
            PARAM_DEBUG << name << ":" << oldValue << "->" << value;
        }
        return true;
    }
    return false;
}

bool AppController::setParameters(const QVariantMap &params)
{
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        if (!setParameter(it.key(), it.value())) {
            emit setParametersFinished(false);
            return false;
        }
    }
    emit setParametersFinished(true);
    m_fisrtSetParameter = false;
    return true;
}

bool AppController::validateParameters()
{
    if (!m_driver) {
        return false;
    }
    return m_driver->validateParameters();
}

bool AppController::commitParameters()
{
    if (!m_driver || !m_driver->isConnected()) {
        emit commitParametersFinished(false);
        return false;
    }
    bool ok = m_driver->commitParameters();
    if (ok) {
        for (auto it = m_pendingParameters.constBegin(); it != m_pendingParameters.constEnd(); ++it) {
            m_parameters[it.key()] = it.value();
        }
        m_pendingParameters.clear();
        saveDynamicConfig(m_cameraId, m_parameters);
    } else {
        m_pendingParameters.clear();
    }
    emit commitParametersFinished(ok);
    return ok;
}

// ——— Private Slots ———

void AppController::onDriverFrameReady(const QSharedPointer<QImage> &image,
                                        quint64 timestamp,
                                        int frameNumber,
                                        const QString &cameraId,
                                     const QVariantMap &parameters)
{
    if (!image) {
        return;
    }

    ImageData frame;
    frame.image = *image;
    frame.timestamp = timestamp;
    frame.frameNumber = frameNumber;
    frame.cameraId = cameraId;
    frame.parameters = parameters;

    emit frameReady(frame);
}

void AppController::onDriverCaptureStarted(const QString &cameraId)
{
    Q_UNUSED(cameraId);
    enterAcquiringState();
    emit captureStarted();
}

void AppController::onDriverCaptureStopped(const QString &cameraId)
{
    Q_UNUSED(cameraId);
    setState(CameraState::Connected);
    emit captureStopped();
}

void AppController::onDriverConnectionChanged(bool connected, const QString &cameraId)
{
    Q_UNUSED(cameraId);
    if (connected && m_state != CameraState::Connected) {
        QStringList paramNames = m_driver->parameterNames();
        for (const QString &name : paramNames) {
            ParameterDefinition def = m_driver->parameter(name);
            if (def.isValid()) {
                m_parameters[name] = def.defaultValue;
            }
        }

        QVariantMap savedParams = loadDynamicConfig(m_cameraId);
        for (auto it = savedParams.constBegin(); it != savedParams.constEnd(); ++it) {
            if (m_parameters.contains(it.key())) {
                m_parameters[it.key()] = it.value();
            }
        }

        setState(CameraState::Connected);
    } else {
        setState(CameraState::Disconnected);
    }
}

void AppController::onDriverError(const CameraError &error)
{
    enterErrorState(error);
}