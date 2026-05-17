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

bool CsvFormatHandler::exportSpectrumCsv(const QVector<double> &spectrum, const QString &path)
{
    if (spectrum.isEmpty()) {
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream s(&f);
    s << "Wavelength,Intensity\n";

    for (int i = 0; i < spectrum.size(); ++i) {
        s << QString::number(i, 'f', 6) << ","
          << QString::number(spectrum[i], 'f', 6) << "\n";
    }

    return true;
}

bool CsvFormatHandler::exportImageCsv(const QImage &img, const QString &path)
{
    if (img.isNull()) {
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream s(&f);
    const int height = img.height();
    const int width = img.width();

    if (img.format() == QImage::Format_Grayscale16) {
        const ushort *bits = reinterpret_cast<const ushort *>(img.bits());
        for (int y = 0; y < height; ++y) {
            QStringList rowValues;
            const ushort *row = bits + y * width;
            for (int x = 0; x < width; ++x) {
                rowValues << QString::number(row[x]);
            }
            s << rowValues.join(",") << "\n";
        }
    } else {
        for (int y = 0; y < height; ++y) {
            QStringList rowValues;
            for (int x = 0; x < width; ++x) {
                QRgb pixel = img.pixel(x, y);
                int gray = qGray(pixel);
                rowValues << QString::number(gray);
            }
            s << rowValues.join(",") << "\n";
        }
    }

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
