#include "DataSaver.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QImageWriter>
#include <QSettings>

#ifdef EZSPECCAM_HAVE_EXIV2
#include <exiv2/exiv2.hpp>
#endif

DataSaver::DataSaver(QObject *parent)
    : QObject(parent)
    , m_frameCounter(0)
    , m_saveOptions()
    , m_frameSaveOptions()
{
}

DataSaver::~DataSaver()
{
}

bool DataSaver::saveFrame(const ImageData &frame, const QString &basePath)
{
    return saveFrame(frame, basePath, ++m_frameCounter);
}

bool DataSaver::saveFrame(const ImageData &frame, const QString &basePath, int frameNumber)
{
    if (!frame.isValid()) {
        emit saveError("Invalid frame data - cannot save");
        return false;
    }

    switch (m_frameSaveOptions.frameFormat) {
    case MetaDataSaveFormat::Separate:
        return saveFrameSeparate(frame, basePath, frameNumber);
    case MetaDataSaveFormat::Embedded:
        return saveFrameEmbedded(frame, basePath, frameNumber);
    }

    return false;
}

bool DataSaver::saveFrameSeparate(const ImageData &frame, const QString &basePath, int frameNumber)
{
    QString fullPath = basePath;

    if (!fullPath.isEmpty() && !fullPath.endsWith(QDir::separator())) {
        fullPath += QDir::separator();
    }

    if (!createDirectory(fullPath)) {
        return false;
    }

    QString imagePath = generateImagePath(fullPath, frameNumber);
    QString configPath = generateConfigPath(fullPath, frameNumber);

    if (!saveImage(frame.image, imagePath)) {
        emit saveError(QString("Failed to save image: %1").arg(imagePath));
        return false;
    }

    QVariantMap parameters = frame.parameters;
    QVariantMap metadata;
    metadata["timestamp"] = static_cast<qlonglong>(frame.timestamp);
    metadata["frameNumber"] = frameNumber;
    metadata["softwareVersion"] = "EZSpecCam 2.0.0";

    if (!saveConfig(configPath, frame.cameraId, parameters, metadata)) {
        emit saveError(QString("Failed to save config: %1").arg(configPath));
        return false;
    }

    emit frameSaved(imagePath, true);
    return true;
}

bool DataSaver::saveFrameEmbedded(const ImageData &frame, const QString &basePath, int frameNumber)
{
    QString fullPath = basePath;
    if (!fullPath.endsWith(QDir::separator())) {
        fullPath += QDir::separator();
    }

    if (!createDirectory(fullPath)) {
        return false;
    }

    QString ext = m_saveOptions.fileExtension();
    QString imagePath = QString("%1img_%2.%3")
        .arg(fullPath)
        .arg(frameNumber, 12, 10, QChar('0'))
        .arg(ext);

    qInfo() << "DataSaver: Saving" << ext << "with embedded XMP to:" << imagePath;

    QImage imgToSave = frame.image;
    if (imgToSave.format() == QImage::Format_Grayscale16) {
        imgToSave = imgToSave.convertToFormat(QImage::Format_Grayscale8);
    }

    QByteArray formatBytes = formatToByteArray(m_saveOptions.format);
    if (!imgToSave.save(imagePath, formatBytes)) {
        emit saveError(QString("Failed to save %1: %2").arg(ext, imagePath));
        return false;
    }

    qInfo() << "DataSaver:" << ext << "saved successfully:" << imagePath;

    QVariantMap params = frame.parameters;
    params["cameraId"] = frame.cameraId;
    params["timestamp"] = static_cast<qlonglong>(frame.timestamp);
    params["frameNumber"] = frameNumber;
    params["softwareVersion"] = "EZSpecCam 2.0.0";

#ifdef EZSPECCAM_HAVE_EXIV2
    bool xmpResult = embedXmpIntoImage(imagePath, params, ext);

    if (!xmpResult) {
        qWarning() << "DataSaver: Failed to embed XMP metadata into" << ext;
    }
#else
    Q_UNUSED(params);
    Q_UNUSED(ext);
#endif

    emit frameSaved(imagePath, true);
    return true;
}

#ifdef EZSPECCAM_HAVE_EXIV2
bool DataSaver::embedXmpIntoImage(const QString &imagePath, const QVariantMap &parameters, const QString &format)
{
    try {
        std::string path = imagePath.toStdString();
        Exiv2::Image::UniquePtr image = Exiv2::ImageFactory::open(path);

        if (!image.get()) {
            qWarning() << "DataSaver: Failed to open" << format << "with Exiv2:" << imagePath;
            return false;
        }

        image->readMetadata();

        Exiv2::XmpData xmpData;

        xmpData["Xmp.xmp.CameraID"] = parameters.value("cameraId").toString().toStdString();
        xmpData["Xmp.xmp.ExposureTime"] = parameters.value("exposure").toDouble();
        xmpData["Xmp.xmp.Gain"] = parameters.value("gain").toDouble();
        xmpData["Xmp.xmp.Temperature"] = parameters.value("temperature").toDouble();
        xmpData["Xmp.xmp.ROILeft"] = parameters.value("roi_x").toInt();
        xmpData["Xmp.xmp.ROITop"] = parameters.value("roi_y").toInt();
        xmpData["Xmp.xmp.ROIWidth"] = parameters.value("roi_width").toInt();
        xmpData["Xmp.xmp.ROIHeight"] = parameters.value("roi_height").toInt();
        xmpData["Xmp.xmp.FrameNumber"] = parameters.value("frameNumber").toInt();
        xmpData["Xmp.xmp.Timestamp"] = parameters.value("timestamp").toLongLong();
        xmpData["Xmp.xmp.Software"] = parameters.value("softwareVersion").toString().toStdString();

        xmpData["Xmp.dc.description"] = parameters.value("cameraId").toString().toStdString();

        image->setXmpData(xmpData);

        image->writeMetadata();

        qInfo() << "DataSaver: XMP metadata embedded into" << format << "successfully";
        return true;

    } catch (const Exiv2::Error &e) {
        qWarning() << "DataSaver: Exiv2 error embedding XMP into" << format << ":" << QString::fromStdString(e.what());
        return false;
    } catch (const std::exception &e) {
        qWarning() << "DataSaver: std::exception embedding XMP into" << format << ":" << e.what();
        return false;
    }
}
#endif

bool DataSaver::saveImage(const QImage &image, const QString &filePath)
{
    if (image.isNull()) {
        qWarning() << "DataSaver: Cannot save null image";
        return false;
    }

    QByteArray format = formatToByteArray(m_saveOptions.format);
    QImageWriter writer(filePath, format);

    if (m_saveOptions.format == ImageFormat::JPEG) {
        writer.setQuality(m_saveOptions.quality);
    }

    if (writer.write(image)) {
        return true;
    }

    qWarning() << "DataSaver: Failed to save image" << filePath << "-" << writer.errorString();
    return false;
}

QByteArray DataSaver::formatToByteArray(ImageFormat format) const
{
    switch (format) {
    case ImageFormat::TIFF: return "TIFF";
    case ImageFormat::JPEG: return "JPEG";
    }
    return "TIFF";
}

QString DataSaver::createTimestampDirectory() const
{
    QDateTime now = QDateTime::currentDateTime();
    return now.toString("yyyy-MM-dd-hh-mm-ss");
}

bool DataSaver::createDirectory(const QString &path)
{
    QDir dir(path);
    if (dir.exists()) {
        return true;
    }

    if (dir.mkpath(".")) {
        return true;
    }

    emit directoryError(path, QString("Failed to create directory: %1").arg(path));
    return false;
}

QString DataSaver::generateImagePath(const QString &directory, int frameNumber) const
{
    QString ext = m_saveOptions.fileExtension();
    return QString("%1%2img_%3.%4")
        .arg(directory)
        .arg(QDir::separator())
        .arg(frameNumber, 12, 10, QChar('0'))
        .arg(ext);
}

QString DataSaver::generateConfigPath(const QString &directory, int frameNumber) const
{
    return QString("%1%2cfg_%3.ini")
        .arg(directory)
        .arg(QDir::separator())
        .arg(frameNumber, 12, 10, QChar('0'));
}

int DataSaver::frameCounter() const
{
    return m_frameCounter;
}

void DataSaver::resetFrameCounter()
{
    m_frameCounter = 0;
}

void DataSaver::setSaveOptions(const ImageSaveOptions &options)
{
    m_saveOptions = options;
}

ImageSaveOptions DataSaver::saveOptions() const
{
    return m_saveOptions;
}

void DataSaver::setFrameSaveOptions(const FrameSaveOptions &options)
{
    m_frameSaveOptions = options;
    m_saveOptions = options;
}

FrameSaveOptions DataSaver::frameSaveOptions() const
{
    return m_frameSaveOptions;
}

bool DataSaver::saveConfig(const QString &configPath, const QString &cameraId,
                         const QVariantMap &parameters, const QVariantMap &metadata)
{
    QFileInfo fileInfo(configPath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            return false;
        }
    }

    QSettings settings(configPath, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) {
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

    settings.beginGroup("Metadata");
    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
        settings.setValue(it.key(), variantToString(it.value()));
    }
    settings.endGroup();

    settings.sync();
    return settings.status() == QSettings::NoError;
}

QString DataSaver::variantToString(const QVariant &value) const
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
