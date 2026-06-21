#ifndef IIMAGEFORMATHANDLER_H
#define IIMAGEFORMATHANDLER_H

#include <QString>
#include <QStringList>
#include <QObject>

#include "SaveTypes.h"

/**
 * @class IImageFormatHandler
 * @brief 抽象接口 - 所有格式处理器必须实现此接口
 */
class IImageFormatHandler : public QObject
{
    Q_OBJECT

public:
    explicit IImageFormatHandler(QObject *parent = nullptr) : QObject(parent) {}
    ~IImageFormatHandler() override = default;

    /// @brief 处理保存请求
    /// @return true on success
    virtual bool save(const SaveRequest &request) = 0;

    /// @brief 是否能处理给定路径 (根据扩展名)
    virtual bool canHandle(const QString &filePath) const = 0;

    /// @brief 支持的扩展名列表
    virtual QStringList supportedExtensions() const = 0;

    /// @brief 用户可见的格式名称 (用于文件对话框过滤器)
    virtual QString displayName() const = 0;
};

#endif // IIMAGEFORMATHANDLER_H
