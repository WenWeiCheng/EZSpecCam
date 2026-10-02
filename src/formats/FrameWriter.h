#pragma once

#include <QString>
#include <QStringList>

#include "CameraTypes.h"

namespace app::formats
{

/// Save a frame to disk; dispatch by file extension. Returns true on success.
bool saveFrame(const ImageData &frame, const QString &filePath);

QStringList supportedSaveExtensions();

/**
 * @brief Compose "img_yyyyMMdd_hhmmss_zzz_<tag>.ext" with optional prefix/suffix.
 *
 * @p tag is a four-symbol hash of @p frameTimestamp and @p frameNumber. The
 * millisecond stamp alone is not enough: a driver may deliver frames faster
 * than one per millisecond, and two frames sharing a stamp would otherwise
 * overwrite each other on disk with no error. Both are passed so the name
 * differs exactly when the stamp does not.
 */
QString generateFilename(const QString &outputDir,
                         const QString &prefix,
                         const QString &suffix,
                         const QString &extension,
                         quint64 frameTimestamp,
                         int frameNumber);

}
