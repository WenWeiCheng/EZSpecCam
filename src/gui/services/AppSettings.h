#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QString>

//! Typed access to the application's QSettings keys.
//!
//! The key strings live only in AppSettings.cpp. Widgets and services call these
//! getters/setters instead of constructing QSettings themselves, so a key can be
//! renamed or migrated in one place. Per-camera driver configuration is separate:
//! that is AppController's INI file, not application preferences.
namespace AppSettings
{

bool autoSaveEnabled();
void setAutoSaveEnabled(bool enabled);

QString autoSaveDirectory();
void setAutoSaveDirectory(const QString &directory);

QString imageFormat();
void setImageFormat(const QString &format);

QString filenamePrefix();
void setFilenamePrefix(const QString &prefix);

QString filenameSuffix();
void setFilenameSuffix(const QString &suffix);

QString lastSaveAsDirectory();
void setLastSaveAsDirectory(const QString &directory);

QString lastOpenDirectory();
void setLastOpenDirectory(const QString &directory);

QString pluginDirectory();
void setPluginDirectory(const QString &directory);

QString calibrationSaveDirectory();
void setCalibrationSaveDirectory(const QString &directory);

//! "light", "dark", or an empty string for "follow the system".
QString themeMode();
void setThemeMode(const QString &mode);

QString historyExportDirectory();
void setHistoryExportDirectory(const QString &directory);

} // namespace AppSettings

#endif // APPSETTINGS_H
