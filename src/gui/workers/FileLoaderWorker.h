#ifndef FILELOADERWORKER_H
#define FILELOADERWORKER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

#include "CameraTypes.h"
#include "formats/SaveTypes.h"

/**
 * @struct LoadResult
 * @brief Loader worker 输出的加载结果
 */
struct LoadResult
{
    bool success = false;
    QString errorMessage;
    ImageData frame;
    bool hasMetadata = false;
    bool hasOriginal = false;
};

/**
 * @class FileLoaderWorker
 * @brief 后台线程读取图像 + 同名 _metadata.json
 *
 * 调用方通过 QMetaObject::invokeMethod + QueuedConnection 提交 loadFrame。
 * 完成后通过 frameLoaded / loadFailed 信号返回。
 */
class FileLoaderWorker : public QObject
{
    Q_OBJECT

public:
    explicit FileLoaderWorker(QObject *parent = nullptr);
    ~FileLoaderWorker() override;

    /// @brief 支持的图像扩展名 (供 QFileDialog 过滤器使用)
    static QStringList supportedOpenExtensions();

    /// @brief 支持的图像格式显示名
    static QString openFormatsDisplayName();

public slots:
    void loadFrame(const QString &filePath);

signals:
    void frameLoaded(const LoadResult &result, const QString &filePath);
    void loadFailed(const QString &error, const QString &filePath);

private:
    bool loadTiff(const QString &filePath, ImageData &out, QString &err) const;
    bool loadCsv(const QString &filePath, ImageData &out, QString &err) const;
    bool loadMetadataSidecar(const QString &imgPath, ImageData &frame, QString &err) const;
    QString metadataSidecarPath(const QString &imgPath) const;
    bool loadImageByExtension(const QString &filePath, ImageData &out, QString &err) const;
};

Q_DECLARE_METATYPE(LoadResult)

#endif // FILELOADERWORKER_H
