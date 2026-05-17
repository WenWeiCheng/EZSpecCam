#ifndef TIFFORMATHANDLER_H
#define TIFFORMATHANDLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>

#include "../IImageFormatHandler.h"
#include "../SaveTypes.h"

/**
 * @class TiffFormatHandler
 * @brief TIFF 格式处理器 - 处理 TIFF 图像保存
 *
 * 修复: saveOriginal 在 TIFF 路径下失效的问题
 */
class TiffFormatHandler : public IImageFormatHandler
{
    Q_OBJECT

public:
    explicit TiffFormatHandler(QObject *parent = nullptr);
    ~TiffFormatHandler() override = default;

    bool save(const SaveRequest &request) override;
    bool canHandle(const QString &filePath) const override;
    QStringList supportedExtensions() const override;
    QString displayName() const override;

private:
    bool saveImage(const QImage &img, const QString &path);
    bool saveMetadataJson(const QString &imgPath, const SaveRequest &request);
    QString insertSuffix(const QString &filePath, const QString &suffix) const;
};

#endif // TIFFORMATHANDLER_H
