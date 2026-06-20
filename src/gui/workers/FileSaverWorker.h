#ifndef FILESAVERWORKER_H
#define FILESAVERWORKER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QFileInfo>
#include <QDir>
#include <vector>
#include <memory>

#include "formats/IImageFormatHandler.h"
#include "formats/SaveTypes.h"

/**
 * @class FileSaverWorker
 * @brief 格式注册中心 + 分发器
 *
 * 负责注册格式处理器、根据路径分发保存请求。
 * 所有阻塞 I/O 在此线程执行。
 */
class FileSaverWorker : public QObject
{
    Q_OBJECT

public:
    explicit FileSaverWorker(QObject *parent = nullptr);
    ~FileSaverWorker() override;

    /// @brief 注册格式处理器 ( ownership 转移 )
    void registerHandler(std::unique_ptr<IImageFormatHandler> handler);

    /// @brief 获取所有可用格式的显示名称列表 (用于文件对话框)
    QStringList availableFormatNames() const;

public slots:
    /// @brief 统一保存入口 - 根据 request.filePath 自动选择 Handler
    void saveFrame(const SaveRequest &request);

signals:
    void completed(const QString &path);
    void failed(const QString &error, const QString &details);

private:
    IImageFormatHandler *findHandler(const QString &filePath) const;

    std::vector<std::unique_ptr<IImageFormatHandler>> m_handlers;      // ownership
    QHash<QString, IImageFormatHandler*> m_extensionMap;            // ext -> handler
};

#endif // FILESAVERWORKER_H
