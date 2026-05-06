#ifndef DATASAVER_H
#define DATASAVER_H

#include <QObject>
#include <QString>
#include <QDir>
#include <QImage>
#include <QSettings>

#include "interfaces/CameraTypes.h"

class DataSaver : public QObject
{
    Q_OBJECT

public:
    explicit DataSaver(QObject *parent = nullptr);
    ~DataSaver() override;

    bool saveFrame(const ImageData &frame, const QString &basePath);
    bool saveFrame(const ImageData &frame, const QString &basePath, int frameNumber);
    bool saveImage(const QImage &image, const QString &filePath);
    int frameCounter() const;
    void resetFrameCounter();
    void setSaveOptions(const ImageSaveOptions &options);
    ImageSaveOptions saveOptions() const;
    void setFrameSaveOptions(const FrameSaveOptions &options);
    FrameSaveOptions frameSaveOptions() const;

signals:
    void frameSaved(const QString &filePath, bool success);
    void saveError(const QString &error);
    void directoryError(const QString &path, const QString &error);

private:
    int m_frameCounter;
    ImageSaveOptions m_saveOptions;
    FrameSaveOptions m_frameSaveOptions;

    bool saveFrameSeparate(const ImageData &frame, const QString &basePath, int frameNumber);
    bool saveFrameEmbedded(const ImageData &frame, const QString &basePath, int frameNumber);

#ifdef EZSPECCAM_HAVE_EXIV2
    bool embedXmpIntoImage(const QString &imagePath, const QVariantMap &parameters, const QString &format);
#endif

    QByteArray formatToByteArray(ImageFormat format) const;
    QString createTimestampDirectory() const;
    bool createDirectory(const QString &path);
    QString generateImagePath(const QString &directory, int frameNumber) const;
    QString generateConfigPath(const QString &directory, int frameNumber) const;

    bool saveConfig(const QString &configPath, const QString &cameraId,
                   const QVariantMap &parameters, const QVariantMap &metadata);
    QString variantToString(const QVariant &value) const;
};

#endif
