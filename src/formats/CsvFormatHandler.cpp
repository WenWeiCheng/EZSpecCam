#include "CsvFormatHandler.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QImage>
#include <QRgb>
#include <QJsonObject>
#include <QJsonDocument>

CsvFormatHandler::CsvFormatHandler(QObject *parent)
    : IImageFormatHandler(parent)
{
}

bool CsvFormatHandler::save(const SaveRequest &request)
{
    const auto &frame = request.frame;

    // 新格式：主图像始终是 original 2D 图（无 original 时退化为 image）
    const QImage &mainImage = frame.hasOriginal() ? frame.originalImage : frame.image;
    if (!exportImageCsv(mainImage, request.filePath)) {
        return false;
    }

    if (!saveMetadataJson(request.filePath, request)) {
        return false;
    }

    return true;
}

bool CsvFormatHandler::canHandle(const QString &filePath) const
{
    QString ext = QFileInfo(filePath).suffix().toLower();
    return ext == "csv";
}

QStringList CsvFormatHandler::supportedExtensions() const
{
    return {"csv"};
}

QString CsvFormatHandler::displayName() const
{
    return QStringLiteral("CSV File (*.csv)");
}

bool CsvFormatHandler::exportImageCsv(const QImage &img, const QString &path)
{
    if (img.isNull()) {
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }

    const int height = img.height();
    const int width = img.width();
    const int lineEstimate = width * 7 + 1;
    QString content;
    content.reserve(height * lineEstimate);

    if (img.format() == QImage::Format_Grayscale16) {
        const ushort *bits = reinterpret_cast<const ushort *>(img.bits());
        const int bytesPerRow = img.bytesPerLine();
        for (int y = 0; y < height; ++y) {
            const ushort *row = bits + y * (bytesPerRow / sizeof(ushort));
            for (int x = 0; x < width; ++x) {
                if (x > 0) content += ',';
                content += QString::number(row[x]);
            }
            content += '\n';
        }
    } else {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (x > 0) content += ',';
                QRgb pixel = img.pixel(x, y);
                content += QString::number(qGray(pixel));
            }
            content += '\n';
        }
    }

    f.write(content.toUtf8());
    return true;
}

bool CsvFormatHandler::saveMetadataJson(const QString &imgPath, const SaveRequest &request)
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
