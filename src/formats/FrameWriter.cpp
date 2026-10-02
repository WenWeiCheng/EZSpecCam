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

namespace
{

/**
 * @brief Four-symbol tag that separates frames the millisecond clock cannot.
 *
 * The timestamp in the name has millisecond resolution, but a driver can hand
 * frames over far faster than that: at 1024x1 the whole frame is 2 KB, so a
 * burst drained out of the camera arrives back-to-back and consecutive frames
 * land in the same millisecond. The second one then silently overwrites the
 * first, which loses data with no error anywhere.
 *
 * Seeded from the frame's own timestamp and frame number rather than from the
 * write clock, so it differs exactly when the millisecond stamp does not.
 *
 * Base 36 rather than base 62: uppercase folds to lowercase on
 * case-insensitive filesystems, quietly halving the space again, and the tag
 * has to come out identical on every platform the CLI runs on.
 */
QString frameTag(quint64 timestamp, int frameNumber)
{
    static const char kBase36[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    // FNV-1a over the two values, byte-wise, so a small change in either moves
    // the whole tag rather than one digit of it.
    quint64 h = 1469598103934665603ULL;
    for (int shift = 0; shift < 64; shift += 8) {
        h = (h ^ ((timestamp >> shift) & 0xFFULL)) * 1099511628211ULL;
    }
    const quint32 number = static_cast<quint32>(frameNumber);
    for (int shift = 0; shift < 32; shift += 8) {
        h = (h ^ ((number >> shift) & 0xFFULL)) * 1099511628211ULL;
    }
    h ^= h >> 32;

    QString tag(4, QLatin1Char('0'));
    for (int i = 3; i >= 0; --i) {
        tag[i] = QLatin1Char(kBase36[h % 36]);
        h /= 36;
    }
    return tag;
}

} // namespace

QString generateFilename(const QString &outputDir,
                         const QString &prefix,
                         const QString &suffix,
                         const QString &extension,
                         quint64 frameTimestamp,
                         int frameNumber)
{
    QString ext = extension.toLower();
    if (ext.isEmpty()) ext = "tiff";

    QString name;
    if (!prefix.isEmpty()) name += prefix + "_";
    name += "img_";
    name += QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    name += "_" + frameTag(frameTimestamp, frameNumber);
    if (!suffix.isEmpty()) name += "_" + suffix;
    name += "." + ext;

    QDir dir(outputDir.isEmpty() ? "." : outputDir);
    return dir.absoluteFilePath(name);
}

}
