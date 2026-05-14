#include "FileSaverWorker.h"

FileSaverWorker::FileSaverWorker(QObject *parent)
    : QObject(parent)
{
}

FileSaverWorker::~FileSaverWorker()
{
}

void FileSaverWorker::saveFrame(const ImageData &frame, const QString &path, bool saveMetadata)
{
    if (frame.image.isNull()) {
        emit failed("Null image");
        return;
    }

    if (!saveImage(frame.image, path)) {
        emit failed(path);
        return;
    }

    if (saveMetadata) {
        saveMetadataJson(path, frame);
    }

    emit completed(path);
}

void FileSaverWorker::exportSpectrumCsv(const QVector<double> &x,
                                        const QVector<double> &y,
                                        const QString &path)
{
    if (x.isEmpty() || y.isEmpty()) {
        emit failed("Empty data");
        return;
    }

    if (saveSpectrumCsv(x, y, path)) {
        emit completed(path);
    } else {
        emit failed(path);
    }
}

void FileSaverWorker::exportImageCsv(const QImage &img, const QString &path)
{
    if (img.isNull()) {
        emit failed("Null image");
        return;
    }

    if (saveImageCsv(img, path)) {
        emit completed(path);
    } else {
        emit failed(path);
    }
}

bool FileSaverWorker::saveImage(const QImage &img, const QString &path)
{
    QFileInfo fi(path);
    QDir(fi.absoluteDir()).mkpath(".");

    QString fmt = fi.suffix().toUpper();
    if (fmt != "PNG" && fmt != "JPG" && fmt != "JPEG") {
        fmt = "TIFF";
    }

    return img.save(path, fmt.toLocal8Bit().constData());
}

bool FileSaverWorker::saveSpectrumCsv(const QVector<double> &x,
                                       const QVector<double> &y,
                                       const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream s(&f);
    s << "Wavelength,Intensity\n";

    int count = qMin(x.size(), y.size());
    for (int i = 0; i < count; ++i) {
        s << QString::number(x[i], 'f', 6) << ","
          << QString::number(y[i], 'f', 6) << "\n";
    }

    return true;
}

bool FileSaverWorker::saveImageCsv(const QImage &img, const QString &path)
{
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

bool FileSaverWorker::saveMetadataJson(const QString &imgPath, const ImageData &frame)
{
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