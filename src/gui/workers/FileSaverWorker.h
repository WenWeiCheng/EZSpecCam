#ifndef FILESAVERWORKER_H
#define FILESAVERWORKER_H

#include <QObject>
#include <QString>
#include <QImage>
#include <QVector>
#include <QJsonObject>
#include <QFile>
#include <QTextStream>
#include <QRgb>
#include <QJsonDocument>
#include <QFileInfo>
#include <QDir>

#include "CameraTypes.h"

/**
 * @class FileSaverWorker
 * @brief Worker for non-blocking file save operations
 *
 * Lives on a dedicated QThread. All blocking I/O (QImage::save, QFile::write)
 * runs here to keep the GUI thread responsive.
 *
 * Thread affinity: the QThread this worker is moved to via moveToThread().
 * All public slots execute on that thread. Signals cross threads to MainWindow.
 */
class FileSaverWorker : public QObject
{
    Q_OBJECT

public:
    explicit FileSaverWorker(QObject *parent = nullptr);
    ~FileSaverWorker() override;

public slots:
    /// @brief Save ImageData to file (TIFF by default, format inferred from path)
    /// @param frame Image and metadata to save
    /// @param path Target file path
    /// @param saveMetadata If true, write _metadata.json alongside
    void saveFrame(const ImageData &frame, const QString &path, bool saveMetadata);

    /// @brief Export 1D spectrum to CSV with Wavelength,Intensity header
    void exportSpectrumCsv(const QVector<double> &xData,
                           const QVector<double> &yData,
                           const QString &path);

    /// @brief Export a QImage to CSV (pixel values as rows of integers)
    void exportImageCsv(const QImage &image, const QString &path);

signals:
    /// @brief Emitted when save completes
    void completed(const QString &path);

    /// @brief Emitted when save fails
    void failed(const QString &error);

private:
    bool saveImage(const QImage &img, const QString &path);
    bool saveSpectrumCsv(const QVector<double> &x, const QVector<double> &y, const QString &path);
    bool saveImageCsv(const QImage &img, const QString &path);
    bool saveMetadataJson(const QString &imgPath, const ImageData &frame);
};

#endif // FILESAVERWORKER_H