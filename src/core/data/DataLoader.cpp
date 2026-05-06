#include "DataLoader.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>

#ifdef EZSPECCAM_HAVE_EXIV2
#include <exiv2/exiv2.hpp>
#endif

DataLoader::DataLoader(QObject *parent)
    : QObject(parent)
{
}

DataLoader::~DataLoader()
{
}

ImageData DataLoader::loadFrame(const QString &imagePath, const QString &configPath)
{
    ImageData frame;

    QImage image = loadImage(imagePath);
    if (image.isNull()) {
        return frame;
    }

    QVariantMap parameters = loadParametersFromFile(configPath);

    QString cameraId;
    QFileInfo configFile(configPath);
    if (configFile.exists()) {
        QSettings settings(configPath, QSettings::IniFormat);
        settings.beginGroup("Camera");
        cameraId = settings.value("id").toString();
        settings.endGroup();
    }

    frame.image = image;
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();
    frame.parameters = parameters;
    frame.cameraId = cameraId;

    return frame;
}

QImage DataLoader::loadImage(const QString &path)
{
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        emit loadError(path, "Image file does not exist");
        emit fileNotFound(path);
        return QImage();
    }

    QImage image(path);
    if (image.isNull()) {
        emit loadError(path, "Failed to load image - format not supported or file corrupted");
        return QImage();
    }

    return image;
}

QString DataLoader::findConfigForImage(const QString &imagePath)
{
    QFileInfo imageFile(imagePath);
    if (!imageFile.exists()) {
        emit fileNotFound(imagePath);
        return QString();
    }

    int frameNum = extractFrameNumber(imageFile.fileName());
    if (frameNum < 0) {
        return QString();
    }

    QString configFilename = generateConfigFilename(frameNum);
    QString configPath = imageFile.absolutePath() + QDir::separator() + configFilename;

    if (QFileInfo::exists(configPath)) {
        return configPath;
    }

    return QString();
}

QString DataLoader::findImageForConfig(const QString &configPath)
{
    QFileInfo configFile(configPath);
    if (!configFile.exists()) {
        emit fileNotFound(configPath);
        return QString();
    }

    int frameNum = extractFrameNumber(configFile.fileName());
    if (frameNum < 0) {
        return QString();
    }

    QString imageFilename = generateImageFilename(frameNum);
    QString imageFullPath = configFile.absolutePath() + QDir::separator() + imageFilename;

    if (QFileInfo::exists(imageFullPath)) {
        return imageFullPath;
    }

    return QString();
}

QStringList DataLoader::listImagesInDirectory(const QString &directory) const
{
    QDir dir(directory);
    if (!dir.exists()) {
        return QStringList();
    }

    QStringList filters;
    filters << "img_*.png" << "img_*.tiff" << "img_*.jpg";

    dir.setNameFilters(filters);
    dir.setSorting(QDir::SortFlag::Name);

    QFileInfoList fileList = dir.entryInfoList();
    QStringList result;

    for (const QFileInfo &fileInfo : fileList) {
        result.append(fileInfo.absoluteFilePath());
    }

    return result;
}

QStringList DataLoader::listConfigsInDirectory(const QString &directory) const
{
    QDir dir(directory);
    if (!dir.exists()) {
        return QStringList();
    }

    QStringList filters;
    filters << "cfg_*.ini";

    dir.setNameFilters(filters);
    dir.setSorting(QDir::SortFlag::Name);

    QFileInfoList fileList = dir.entryInfoList();
    QStringList result;

    for (const QFileInfo &fileInfo : fileList) {
        result.append(fileInfo.absoluteFilePath());
    }

    return result;
}

QString DataLoader::getStoredVersion(const QString &configPath) const
{
    QFileInfo fileInfo(configPath);
    if (!fileInfo.exists()) {
        return QString();
    }

    QSettings settings(configPath, QSettings::IniFormat);
    return settings.value("Version", QString()).toString();
}

QString DataLoader::loadCameraId(const QString &configPath) const
{
    QFileInfo fileInfo(configPath);
    if (!fileInfo.exists()) {
        return QString();
    }

    QSettings settings(configPath, QSettings::IniFormat);
    settings.beginGroup("Camera");
    QString cameraId = settings.value("id", QString()).toString();
    settings.endGroup();
    return cameraId;
}

QVariantMap DataLoader::loadParametersFromFile(const QString &configPath) const
{
    QVariantMap parameters;

    QFileInfo fileInfo(configPath);
    if (!fileInfo.exists()) {
        return parameters;
    }

    QSettings settings(configPath, QSettings::IniFormat);

    settings.beginGroup("Parameters");
    const QStringList keys = settings.allKeys();
    for (const QString &key : keys) {
        QString valueStr = settings.value(key).toString();
        int colonPos = valueStr.indexOf(':');
        if (colonPos > 0) {
            QString typePrefix = valueStr.left(colonPos);
            QString actualValue = valueStr.mid(colonPos + 1);

            QVariant value;
            if (typePrefix == "double") {
                value = QVariant(actualValue).toDouble();
            } else if (typePrefix == "int") {
                value = QVariant(actualValue).toInt();
            } else if (typePrefix == "bool") {
                value = QVariant(actualValue).toBool();
            } else {
                value = actualValue;
            }
            parameters.insert(key, value);
        } else {
            parameters.insert(key, valueStr);
        }
    }
    settings.endGroup();

    return parameters;
}

int DataLoader::extractFrameNumber(const QString &filename) const
{
    QRegularExpression rxTiff("img_(\\d+)\\.tiff", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch matchTiff = rxTiff.match(filename);
    if (matchTiff.hasMatch()) {
        return matchTiff.captured(1).toInt();
    }

    QRegularExpression rxJpg("img_(\\d+)\\.jpg", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch matchJpg = rxJpg.match(filename);
    if (matchJpg.hasMatch()) {
        return matchJpg.captured(1).toInt();
    }

    QRegularExpression rxPng("img_(\\d+)\\.png");
    QRegularExpressionMatch matchPng = rxPng.match(filename);
    if (matchPng.hasMatch()) {
        return matchPng.captured(1).toInt();
    }

    QRegularExpression rxCfg("cfg_(\\d+)\\.ini");
    QRegularExpressionMatch matchCfg = rxCfg.match(filename);
    if (matchCfg.hasMatch()) {
        return matchCfg.captured(1).toInt();
    }

    return -1;
}

QString DataLoader::generateConfigFilename(int frameNumber) const
{
    return QString("cfg_%1.ini").arg(frameNumber, 12, 10, QChar('0'));
}

QString DataLoader::generateImageFilename(int frameNumber) const
{
    return QString("img_%1.tiff").arg(frameNumber, 12, 10, QChar('0'));
}

#ifdef EZSPECCAM_HAVE_EXIV2
FrameMetadata DataLoader::loadFromExiv2(const QString &imagePath)
{
    FrameMetadata metadata;

    try {
        std::string pathStd = imagePath.toLocal8Bit().constData();
        Exiv2::Image::UniquePtr image = Exiv2::ImageFactory::open(pathStd);
        if (!image.get()) {
            emit loadError(imagePath, "Failed to open image with Exiv2");
            return metadata;
        }

        image->readMetadata();

        Exiv2::XmpData xmpData = image->xmpData();

        auto getString = [&](const char* key) -> QString {
            if (xmpData.findKey(Exiv2::XmpKey(key)) != xmpData.end()) {
                return QString::fromStdString(xmpData[key].value().toString());
            }
            return QString();
        };

        metadata.cameraId = getString("Xmp.xmp.CameraID");
        metadata.exposure = getString("Xmp.xmp.ExposureTime").toDouble();
        metadata.gain = getString("Xmp.xmp.Gain").toDouble();
        metadata.temperature = getString("Xmp.xmp.Temperature").toDouble();

        bool ok = false;
        metadata.roi.x = getString("Xmp.xmp.ROILeft").toInt(&ok);
        metadata.roi.y = getString("Xmp.xmp.ROITop").toInt(&ok);
        metadata.roi.width = getString("Xmp.xmp.ROIWidth").toInt(&ok);
        metadata.roi.height = getString("Xmp.xmp.ROIHeight").toInt(&ok);

        metadata.frameNumber = getString("Xmp.xmp.FrameNumber").toInt(&ok);
        metadata.timestamp = getString("Xmp.xmp.Timestamp").toLongLong(&ok);

        metadata.softwareVersion = getString("Xmp.xmp.Software");

    } catch (const Exiv2::Error &e) {
        emit loadError(imagePath, QString("Exiv2 error: %1").arg(QString::fromStdString(e.what())));
    }

    return metadata;
}
#endif
