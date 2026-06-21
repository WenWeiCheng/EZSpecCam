#pragma once

#include <QString>

namespace app
{

/// Translate a CLI / sequence-JSON format string ("tiff", "csv", ...) to a
/// file extension ("tiff", "csv"). Unknown / empty values fall back to "tiff"
/// and emit a warning via qWarning.
///
/// This is CLI-input translation policy; it lives outside app::formats because
/// app::formats should only deal in extensions, not in CLI option values.
inline QString cliFormatToExtension(const QString &cliFormat)
{
    const QString f = cliFormat.toLower().trimmed();
    if (f == "csv") return "csv";
    if (!f.isEmpty() && f != "tiff")
        qWarning() << "Unknown format:" << cliFormat << "- using tiff";
    return "tiff";
}

}