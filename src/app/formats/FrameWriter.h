#pragma once

#include <QString>
#include <QStringList>

#include "CameraTypes.h"

namespace app::formats
{

/// Save a frame to disk; dispatch by file extension. Returns true on success.
bool saveFrame(const ImageData &frame, const QString &filePath);

QStringList supportedSaveExtensions();

/// Map CLI --format values to file extensions: "tiff" → "tiff", "csv" → "csv".
/// Unknown values return "tiff".
QString extensionForCliFormat(const QString &cliFormat);

/// Compose "img_yyyyMMdd_hhmmss_zzz.ext" with optional prefix/suffix.
QString generateFilename(const QString &outputDir,
                         const QString &prefix,
                         const QString &suffix,
                         const QString &extension);

}
