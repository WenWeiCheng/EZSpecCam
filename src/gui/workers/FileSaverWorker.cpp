#include "FileSaverWorker.h"

FileSaverWorker::FileSaverWorker(QObject *parent)
    : QObject(parent)
{
}

FileSaverWorker::~FileSaverWorker()
{
}

void FileSaverWorker::registerHandler(std::unique_ptr<IImageFormatHandler> handler)
{
    if (!handler) return;

    for (const QString &ext : handler->supportedExtensions()) {
        m_extensionMap[ext.toLower()] = handler.get();
    }
    m_handlers.push_back(std::move(handler));
}

QStringList FileSaverWorker::availableFormatNames() const
{
    QStringList names;
    for (const auto &handler : m_handlers) {
        names << handler->displayName();
    }
    return names;
}

IImageFormatHandler *FileSaverWorker::findHandler(const QString &filePath) const
{
    QString ext = QFileInfo(filePath).suffix().toLower();
    return m_extensionMap.value(ext, nullptr);
}

void FileSaverWorker::saveFrame(const SaveRequest &request)
{
    if (request.frame.image.isNull()) {
        emit failed("Null image", request.filePath);
        return;
    }

    IImageFormatHandler *handler = findHandler(request.filePath);
    if (!handler) {
        emit failed("Unsupported format", request.filePath);
        return;
    }

    if (handler->save(request)) {
        emit completed(request.filePath);
    } else {
        emit failed("Save failed", request.filePath);
    }
}
