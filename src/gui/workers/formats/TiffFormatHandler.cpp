#include "TiffFormatHandler.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonValue>

TiffFormatHandler::TiffFormatHandler(QObject *parent)
    : IImageFormatHandler(parent)
{
}

bool TiffFormatHandler::save(const SaveRequest &request)
{
    const auto &frame = request.frame;

    // 新格式：主图像始终是 original 2D 图（无 original 时退化为 image）
    const QImage &mainImage = frame.hasOriginal() ? frame.originalImage : frame.image;
    if (!saveImage(mainImage, request.filePath)) {
        return false;
    }

    if (!saveMetadataJson(request.filePath, request)) {
        return false;
    }

    return true;
}

bool TiffFormatHandler::canHandle(const QString &filePath) const
{
    QString ext = QFileInfo(filePath).suffix().toLower();
    return ext == "tiff" || ext == "tif";
}

QStringList TiffFormatHandler::supportedExtensions() const
{
    return {"tiff", "tif"};
}

QString TiffFormatHandler::displayName() const
{
    return QStringLiteral("TIFF Image (*.tiff *.tif)");
}

bool TiffFormatHandler::saveImage(const QImage &img, const QString &path)
{
    if (img.isNull()) {
        return false;
    }

    QFileInfo fi(path);
    QDir(fi.absoluteDir()).mkpath(".");

    return img.save(path, "TIFF");
}

bool TiffFormatHandler::saveMetadataJson(const QString &imgPath, const SaveRequest &request)
{
    const auto &frame = request.frame;

    QFileInfo fileInfo(imgPath);
    QString metadataPath = fileInfo.absoluteDir().absolutePath() + "/" + fileInfo.baseName() + "_metadata.json";

    QJsonObject root;
    root["cameraId"] = frame.cameraId;
    root["timestamp"] = static_cast<qint64>(frame.timestamp);
    root["frameNumber"] = frame.frameNumber;

    QJsonObject paramsObj;
    for (auto it = frame.parameters.constBegin(); it != frame.parameters.constEnd(); ++it) {
        paramsObj[it.key()] = QJsonValue::fromVariant(it.value());
    }
    root["parameters"] = paramsObj;

    QJsonObject softwareObj;
    for (auto it = frame.softwareSettings.constBegin(); it != frame.softwareSettings.constEnd(); ++it) {
        softwareObj[it.key()] = QJsonValue::fromVariant(it.value());
    }
    root["softwareSettings"] = softwareObj;

    QFile file(metadataPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}
