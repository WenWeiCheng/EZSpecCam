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
    const auto &opts = request.options;

    // 保存主图像
    if (!saveImage(frame.image, request.filePath)) {
        return false;
    }

    // 保存原始数据
    if (frame.image.height() == 1 && opts.saveOriginal && frame.hasOriginal()) {
        QString origPath = insertSuffix(request.filePath, "_original");
        if (!saveImage(frame.originalImage, origPath)) {
            return false;
        }
    }

    // 元数据
    if (opts.saveMetadata) {
        if (!saveMetadataJson(request.filePath, request)) {
            return false;
        }
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

QString TiffFormatHandler::insertSuffix(const QString &filePath, const QString &suffix) const
{
    QString result = filePath;
    int dotIndex = result.lastIndexOf('.');
    if (dotIndex > 0) {
        result.insert(dotIndex, suffix);
    } else {
        result += suffix;
    }
    return result;
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

    if (!frame.config.isEmpty()) {
        QJsonObject configObj;
        for (auto it = frame.config.constBegin(); it != frame.config.constEnd(); ++it) {
            configObj[it.key()] = QJsonValue::fromVariant(it.value());
        }
        root["config"] = configObj;
    }

    QFile file(metadataPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}
