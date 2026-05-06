#include "ConfigurationManager.h"
#include "CameraTypes.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QtMath>

ConfigurationManager::ConfigurationManager(QObject *parent)
    : QObject(parent)
{
}

QString ConfigurationManager::getConfigDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/configs";
}

QString ConfigurationManager::getConfigPath(const QString &cameraId)
{
    return getConfigDirectory() + "/" + cameraId + ".ini";
}

void ConfigurationManager::saveDynamicConfig(const QString &cameraId, const QHash<QString, QVariant> &parameters)
{
    QString path = getConfigPath(cameraId);

    QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            QString error = "Failed to create directory for: " + path;
            qWarning() << error;
            emit saveError(path, error);
            return;
        }
    }

    QVariantMap params;
    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        params.insert(it.key(), it.value());
    }

    if (!saveParameters(path, cameraId, params)) {
        QString error = "Failed to save dynamic config to: " + path;
        qWarning() << error;
        emit saveError(path, error);
    }
}

QHash<QString, QVariant> ConfigurationManager::loadDynamicConfig(const QString &cameraId)
{
    QHash<QString, QVariant> parameters;
    QString path = getConfigPath(cameraId);

    QFile file(path);
    if (!file.exists()) {
        return parameters;
    }

    QString camId;
    QVariantMap params;
    if (!loadParameters(path, camId, params)) {
        qWarning() << "Failed to load dynamic config file:" << path;
        emit loadError(path, "Failed to load config");
        return parameters;
    }

    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        parameters.insert(it.key(), it.value());
    }

    return parameters;
}

bool ConfigurationManager::ensureDirectoryExists(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        return dir.mkpath(".");
    }
    return true;
}

QString ConfigurationManager::variantToString(const QVariant &value)
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

QVariant ConfigurationManager::stringToVariant(const QString &valueStr)
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

bool ConfigurationManager::saveParameters(const QString &filePath,
                                        const QString &cameraId,
                                        const QVariantMap &parameters)
{
    if (!ensureDirectoryExists(filePath)) {
        qWarning() << "ConfigurationManager: Failed to create directory for" << filePath;
        return false;
    }

    QSettings settings(filePath, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) {
        qWarning() << "ConfigurationManager: Failed to create QSettings for" << filePath;
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

bool ConfigurationManager::loadParameters(const QString &filePath,
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

bool ConfigurationManager::saveMetadata(const QString &filePath,
                                        const QVariantMap &metadata)
{
    if (!ensureDirectoryExists(filePath)) {
        return false;
    }

    QSettings settings(filePath, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) {
        return false;
    }

    settings.beginGroup("Metadata");
    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
        settings.setValue(it.key(), variantToString(it.value()));
    }
    settings.endGroup();

    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool ConfigurationManager::loadMetadata(const QString &filePath,
                                        QVariantMap &metadata)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        return false;
    }

    QSettings settings(filePath, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) {
        return false;
    }

    metadata.clear();
    settings.beginGroup("Metadata");
    const QStringList keys = settings.allKeys();
    for (const QString &key : keys) {
        metadata.insert(key, stringToVariant(settings.value(key).toString()));
    }
    settings.endGroup();

    return true;
}

bool ConfigurationManager::validate(const QVariant &value,
                                   const ParameterConstraint &constraint,
                                   ParameterType type)
{
    switch (type) {
    case ParameterType::FloatRange: {
        double val = value.toDouble();
        return val >= constraint.minValue && val <= constraint.maxValue;
    }
    case ParameterType::FloatCollection: {
        double val = value.toDouble();
        for (const QVariant &v : constraint.validValues) {
            if (qAbs(v.toDouble() - val) < 0.0001) {
                return true;
            }
        }
        return false;
    }
    case ParameterType::IntRange: {
        int val = value.toInt();
        return val >= constraint.minValue && val <= constraint.maxValue;
    }
    case ParameterType::IntCollection: {
        int val = value.toInt();
        for (const QVariant &v : constraint.validValues) {
            if (v.toInt() == val) {
                return true;
            }
        }
        return false;
    }
    case ParameterType::String: {
        return value.canConvert<QString>();
    }
    case ParameterType::StringCollection: {
        QString val = value.toString();
        for (const QVariant &v : constraint.validValues) {
            if (v.toString() == val) {
                return true;
            }
        }
        return false;
    }
    case ParameterType::Boolean: {
        return value.canConvert<bool>();
    }
    default:
        return true;
    }
}

QString ConfigurationManager::validateReason(const QVariant &value,
                                            const ParameterConstraint &constraint,
                                            ParameterType type)
{
    switch (type) {
    case ParameterType::FloatRange: {
        double val = value.toDouble();
        if (val < constraint.minValue) {
            return QString("Value %1 is below minimum %2").arg(val).arg(constraint.minValue);
        }
        if (val > constraint.maxValue) {
            return QString("Value %1 is above maximum %2").arg(val).arg(constraint.maxValue);
        }
        return QString();
    }
    case ParameterType::FloatCollection: {
        double val = value.toDouble();
        bool found = false;
        for (const QVariant &v : constraint.validValues) {
            if (qAbs(v.toDouble() - val) < 0.0001) {
                found = true;
                break;
            }
        }
        if (!found) {
            return QString("Value %1 is not in valid collection").arg(val);
        }
        return QString();
    }
    case ParameterType::IntRange: {
        int val = value.toInt();
        if (val < constraint.minValue) {
            return QString("Value %1 is below minimum %2").arg(val).arg(static_cast<int>(constraint.minValue));
        }
        if (val > constraint.maxValue) {
            return QString("Value %1 is above maximum %2").arg(val).arg(static_cast<int>(constraint.maxValue));
        }
        return QString();
    }
    case ParameterType::IntCollection: {
        int val = value.toInt();
        bool found = false;
        for (const QVariant &v : constraint.validValues) {
            if (v.toInt() == val) {
                found = true;
                break;
            }
        }
        if (!found) {
            return QString("Value %1 is not in valid collection").arg(val);
        }
        return QString();
    }
    case ParameterType::String: {
        if (!value.canConvert<QString>()) {
            return QString("Value cannot be converted to string");
        }
        return QString();
    }
    case ParameterType::StringCollection: {
        QString val = value.toString();
        bool found = false;
        for (const QVariant &v : constraint.validValues) {
            if (v.toString() == val) {
                found = true;
                break;
            }
        }
        if (!found) {
            return QString("Value '%1' is not in valid collection").arg(val);
        }
        return QString();
    }
    case ParameterType::Boolean: {
        if (!value.canConvert<bool>()) {
            return QString("Value cannot be converted to boolean");
        }
        return QString();
    }
    default:
        return QString();
    }
}
