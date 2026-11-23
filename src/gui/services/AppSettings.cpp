#include "AppSettings.h"

#include <QSettings>

namespace
{

QString readString(const char *key, const QString &defaultValue = QString())
{
    const QSettings settings;
    return settings.value(QString::fromLatin1(key), defaultValue).toString();
}

void writeString(const char *key, const QString &value)
{
    QSettings settings;
    settings.setValue(QString::fromLatin1(key), value);
}

bool readBool(const char *key, bool defaultValue = false)
{
    const QSettings settings;
    return settings.value(QString::fromLatin1(key), defaultValue).toBool();
}

void writeBool(const char *key, bool value)
{
    QSettings settings;
    settings.setValue(QString::fromLatin1(key), value);
}

} // namespace

namespace AppSettings
{

bool autoSaveEnabled()
{
    return readBool("data/autoSaveEnabled", false);
}

void setAutoSaveEnabled(bool enabled)
{
    writeBool("data/autoSaveEnabled", enabled);
}

QString autoSaveDirectory()
{
    return readString("data/autoSaveDirectory");
}

void setAutoSaveDirectory(const QString &directory)
{
    writeString("data/autoSaveDirectory", directory);
}

QString imageFormat()
{
    return readString("data/imageFormat", QStringLiteral("TIFF"));
}

void setImageFormat(const QString &format)
{
    writeString("data/imageFormat", format);
}

QString filenamePrefix()
{
    return readString("data/filenamePrefix");
}

void setFilenamePrefix(const QString &prefix)
{
    writeString("data/filenamePrefix", prefix);
}

QString filenameSuffix()
{
    return readString("data/filenameSuffix");
}

void setFilenameSuffix(const QString &suffix)
{
    writeString("data/filenameSuffix", suffix);
}

QString lastSaveAsDirectory()
{
    return readString("data/lastSaveAsDirectory");
}

void setLastSaveAsDirectory(const QString &directory)
{
    writeString("data/lastSaveAsDirectory", directory);
}

QString lastOpenDirectory()
{
    return readString("data/lastOpenDirectory");
}

void setLastOpenDirectory(const QString &directory)
{
    writeString("data/lastOpenDirectory", directory);
}

QString pluginDirectory()
{
    return readString("plugins/pluginDirectory");
}

void setPluginDirectory(const QString &directory)
{
    writeString("plugins/pluginDirectory", directory);
}

QString calibrationSaveDirectory()
{
    return readString("calibration/lastSaveDirectory");
}

void setCalibrationSaveDirectory(const QString &directory)
{
    writeString("calibration/lastSaveDirectory", directory);
}

QString themeMode()
{
    return readString("ui/themeMode");
}

void setThemeMode(const QString &mode)
{
    if (mode.isEmpty()) {
        QSettings settings;
        settings.remove(QStringLiteral("ui/themeMode"));
        return;
    }
    writeString("ui/themeMode", mode);
}

QString historyExportDirectory()
{
    return readString("parameterHistory/lastExportDirectory");
}

void setHistoryExportDirectory(const QString &directory)
{
    writeString("parameterHistory/lastExportDirectory", directory);
}

} // namespace AppSettings
