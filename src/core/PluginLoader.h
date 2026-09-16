#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

class ICameraDriver;

namespace app::plugins
{

struct Entry
{
    QString filePath;
    QStringList cameraIds;
    ICameraDriver *instance = nullptr;
};

using LoadFailedCallback = std::function<void(const QString &filePath, const QString &error)>;
using ScanProgressCallback = std::function<void(int current, int total, const QString &currentFile)>;

void setExtraRoots(const QStringList &roots);
void setLoadFailedCallback(LoadFailedCallback cb);
void setScanProgressCallback(ScanProgressCallback cb);
int  scanDefaultRoots();
int  scan(const QStringList &roots);
const QVector<Entry> &entries();
const Entry *findByCamera(const QString &cameraId);
QStringList enumerateCameras();
void unloadAll();

}
