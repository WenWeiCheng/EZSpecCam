#ifndef DATALOADER_H
#define DATALOADER_H

#include <QObject>
#include <QString>
#include <QDir>
#include <QImage>
#include <QSettings>
#include <QFileInfoList>
#include <QStringList>
#include <QVariantMap>
#include <QRegularExpression>

#include "interfaces/CameraTypes.h"

class DataLoader : public QObject
{
    Q_OBJECT

public:
    explicit DataLoader(QObject *parent = nullptr);
    ~DataLoader() override;

    ImageData loadFrame(const QString &imagePath, const QString &configPath);
    QImage loadImage(const QString &path);
    QString findConfigForImage(const QString &imagePath);
    QString findImageForConfig(const QString &configPath);
    QStringList listImagesInDirectory(const QString &directory) const;
    QStringList listConfigsInDirectory(const QString &directory) const;
    QString getStoredVersion(const QString &configPath) const;
    QString loadCameraId(const QString &configPath) const;

signals:
    void loadError(const QString &path, const QString &error);
    void fileNotFound(const QString &path);
    void versionMismatch(const QString &storedVersion, const QString &currentVersion);

public:
    int extractFrameNumber(const QString &filename) const;
    QString generateConfigFilename(int frameNumber) const;
    QString generateImageFilename(int frameNumber) const;

private:

#ifdef EZSPECCAM_HAVE_EXIV2
    FrameMetadata loadFromExiv2(const QString &tiffPath);
#endif

    QVariantMap loadParametersFromFile(const QString &configPath) const;
};

#endif
