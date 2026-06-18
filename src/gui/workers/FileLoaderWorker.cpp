#include "FileLoaderWorker.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QImageReader>
#include <QTextStream>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDateTime>

FileLoaderWorker::FileLoaderWorker(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<LoadResult>("LoadResult");
}

FileLoaderWorker::~FileLoaderWorker() = default;

QStringList FileLoaderWorker::supportedOpenExtensions()
{
    return {"tiff", "tif", "csv"};
}

QString FileLoaderWorker::openFormatsDisplayName()
{
    return QStringLiteral("Image Files (*.tiff *.tif *.csv)");
}

void FileLoaderWorker::loadFrame(const QString &filePath)
{
    LoadResult result;
    result.success = false;

    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        result.errorMessage = QStringLiteral("File does not exist: %1").arg(filePath);
        emit loadFailed(result.errorMessage, filePath);
        return;
    }

    QString loadError;
    if (!loadImageByExtension(filePath, result.frame, loadError)) {
        result.errorMessage = loadError;
        emit loadFailed(loadError, filePath);
        return;
    }

    // 新格式：主图即 original 2D 图；加载同目录 _metadata.json（若存在）
    if (loadMetadataSidecar(filePath, result.frame, loadError)) {
        result.hasMetadata = true;
    }

    result.success = true;
    emit frameLoaded(result, filePath);
}

bool FileLoaderWorker::loadTiff(const QString &filePath, ImageData &out, QString &err) const
{
    QImage img;
    if (!img.load(filePath, "TIFF")) {
        err = QStringLiteral("Failed to load TIFF image: %1").arg(filePath);
        return false;
    }
    out.image = img;
    out.timestamp = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()) * 1000000ULL;
    return true;
}

bool FileLoaderWorker::loadCsv(const QString &filePath, ImageData &out, QString &err) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        err = QStringLiteral("Cannot open CSV: %1").arg(filePath);
        return false;
    }

    QStringList lines;
    while (!f.atEnd()) {
        QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (!line.isEmpty()) {
            lines.append(line);
        }
    }
    f.close();

    if (lines.isEmpty()) {
        err = QStringLiteral("Empty CSV file: %1").arg(filePath);
        return false;
    }

    if (lines.first().startsWith(QStringLiteral("Index,"))) {
        QVector<quint64> spectrum;
        for (int i = 1; i < lines.size(); ++i) {
            const QStringList parts = lines[i].split(',');
            if (parts.size() < 2) {
                continue;
            }
            bool ok = false;
            quint64 v = parts[1].toULongLong(&ok);
            if (ok) {
                spectrum.append(v);
            }
        }
        if (spectrum.isEmpty()) {
            err = QStringLiteral("CSV spectrum has no data rows: %1").arg(filePath);
            return false;
        }
        out.spectrum = spectrum;

        const int width = spectrum.size();
        QImage img(1, 1, QImage::Format_Grayscale16);
        ushort *bits = reinterpret_cast<ushort *>(img.bits());
        for (int x = 0; x < width; ++x) {
            bits[x] = static_cast<ushort>(qMin<quint64>(spectrum[x], 65535ULL));
        }
        out.image = img;
    } else {
        const int height = lines.size();
        QStringList firstParts = lines.first().split(',');
        const int width = firstParts.size();
        if (width <= 0) {
            err = QStringLiteral("CSV has no columns: %1").arg(filePath);
            return false;
        }
        QImage img(width, height, QImage::Format_Grayscale16);
        for (int y = 0; y < height; ++y) {
            const QStringList parts = lines[y].split(',');
            if (parts.size() != width) {
                err = QStringLiteral("CSV row %1 has %2 columns, expected %3")
                          .arg(y).arg(parts.size()).arg(width);
                return false;
            }
            ushort *row = reinterpret_cast<ushort *>(img.scanLine(y));
            for (int x = 0; x < width; ++x) {
                bool ok = false;
                int v = parts[x].toInt(&ok);
                row[x] = ok ? static_cast<ushort>(qMax(0, v)) : 0;
            }
        }
        out.image = img;
    }

    out.timestamp = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()) * 1000000ULL;
    return true;
}

QString FileLoaderWorker::metadataSidecarPath(const QString &imgPath) const
{
    QFileInfo fi(imgPath);
    return fi.absoluteDir().absolutePath() + "/" + fi.baseName() + "_metadata.json";
}

bool FileLoaderWorker::loadImageByExtension(const QString &filePath, ImageData &out, QString &err) const
{
    QFileInfo fi(filePath);
    const QString ext = fi.suffix().toLower();
    if (ext == "tiff" || ext == "tif") {
        return loadTiff(filePath, out, err);
    }
    if (ext == "csv") {
        return loadCsv(filePath, out, err);
    }
    err = QStringLiteral("Unsupported format: %1").arg(ext);
    return false;
}

bool FileLoaderWorker::loadMetadataSidecar(const QString &imgPath, ImageData &frame, QString &err) const
{
    Q_UNUSED(err);
    const QString metaPath = metadataSidecarPath(imgPath);
    QFile f(metaPath);
    if (!f.exists()) {
        return false;
    }
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(bytes, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();
    frame.cameraId = root.value("cameraId").toString();
    frame.timestamp = static_cast<quint64>(root.value("timestamp").toVariant().toLongLong());
    frame.frameNumber = root.value("frameNumber").toInt();

    QJsonObject params = root.value("parameters").toObject();
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        frame.parameters.insert(it.key(), it.value().toVariant());
    }

    QJsonObject sw = root.value("softwareSettings").toObject();
    for (auto it = sw.constBegin(); it != sw.constEnd(); ++it) {
        frame.softwareSettings.insert(it.key(), it.value().toVariant());
    }

    return true;
}
