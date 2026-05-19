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
    const auto &opts = request.options;

    if (frame.image.height() == 1) {
        if (!exportSpectrumCsv(frame.spectrum, request.filePath)) {
            return false;
        }
    } else {
        if (!exportImageCsv(frame.image, request.filePath)) {
            return false;
        }
    }

    if (frame.image.height() == 1 && opts.saveOriginal && frame.hasOriginal()) {
        QString origPath = insertSuffix(request.filePath, "_original");
        if (!exportImageCsv(frame.originalImage, origPath)) {
            return false;
        }
    }

    if (opts.saveMetadata) {
        if (!saveMetadataJson(request.filePath, request)) {
            return false;
        }
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

QString CsvFormatHandler::insertSuffix(const QString &filePath, const QString &suffix) const
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

bool CsvFormatHandler::exportSpectrumCsv(const QVector<quint64> &spectrum, const QString &path)
{
    if (spectrum.isEmpty()) {
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QString content;
    content.reserve(spectrum.size() * 25);
    content += "Index,Counts\n";

    for (int i = 0; i < spectrum.size(); ++i) {
        content += QString::number(i);
        content += ',';
        content += QString::number(spectrum[i]);
        content += '\n';
    }

    f.write(content.toUtf8());
    return true;
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
