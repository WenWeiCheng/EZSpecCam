#include "FrameWriter.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

#include "IImageFormatHandler.h"
#include "TiffFormatHandler.h"
#include "CsvFormatHandler.h"

#include <memory>
#include <vector>

namespace
{

std::vector<std::unique_ptr<IImageFormatHandler>> buildHandlers()
{
    std::vector<std::unique_ptr<IImageFormatHandler>> v;
    v.push_back(std::make_unique<TiffFormatHandler>());
    v.push_back(std::make_unique<CsvFormatHandler>());
    return v;
}

IImageFormatHandler *findHandler(const QString &filePath)
{
    static const auto handlers = buildHandlers();
    for (const auto &h : handlers)
    {
        if (h->canHandle(filePath)) return h.get();
    }
    return nullptr;
}

}

namespace app::formats
{

bool saveFrame(const ImageData &frame, const QString &filePath)
{
    IImageFormatHandler *h = findHandler(filePath);
    if (!h) return false;

    SaveRequest req;
    req.frame = frame;
    req.filePath = filePath;
    return h->save(req);
}

QStringList supportedSaveExtensions()
{
    return { "tiff", "tif", "csv" };
}

QString extensionForCliFormat(const QString &cliFormat)
{
    const QString f = cliFormat.toLower();
    if (f == "csv") return "csv";
    return "tiff";
}

QString generateFilename(const QString &outputDir,
                         const QString &prefix,
                         const QString &suffix,
                         const QString &extension)
{
    QString ext = extension.toLower();
    if (ext.isEmpty()) ext = "tiff";

    QString name;
    if (!prefix.isEmpty()) name += prefix + "_";
    name += "img_";
    name += QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    if (!suffix.isEmpty()) name += "_" + suffix;
    name += "." + ext;

    QDir dir(outputDir.isEmpty() ? "." : outputDir);
    return dir.absoluteFilePath(name);
}

}
