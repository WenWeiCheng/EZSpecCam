#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

class ICameraDriver;

namespace app::plugins
{

struct Entry
{
    QString filePath;
    QStringList cameraIds;
    ICameraDriver *instance = nullptr;
};

void setExtraRoots(const QStringList &roots);
int  scanDefaultRoots();
int  scan(const QStringList &roots);
const QVector<Entry> &entries();
const Entry *findByCamera(const QString &cameraId);
QStringList enumerateCameras();
void unloadAll();

}
