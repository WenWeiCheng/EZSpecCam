#include "PluginLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPluginLoader>
#include <QSet>

#include "ICameraDriver.h"

namespace
{

QVector<app::plugins::Entry> g_entries;
QStringList g_extraRoots;
app::plugins::LoadFailedCallback g_loadFailedCallback;
app::plugins::ScanProgressCallback g_scanProgressCallback;

QStringList defaultRoots()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    return { appDir + "/plugins/drivers", appDir + "/../plugins/drivers" };
}

QStringList gatherCandidateFiles(const QStringList &roots, QSet<QString> &seenFiles)
{
    QStringList out;
    for (const QString &root : roots)
    {
        QDir dir(root);
        if (!dir.exists())
            continue;

        const QStringList nameFilters = QStringList()
            << "*.dll" << "*.so" << "*.dylib";
        for (const QFileInfo &fi : dir.entryInfoList(nameFilters, QDir::Files))
        {
            const QString path = fi.absoluteFilePath();
            const QString canon = QFileInfo(path).canonicalFilePath();
            if (seenFiles.contains(canon))
                continue;
            seenFiles.insert(canon);
            out.append(path);
        }
    }
    return out;
}

}

namespace app::plugins
{

void setExtraRoots(const QStringList &roots)
{
    g_extraRoots = roots;
}

void setLoadFailedCallback(LoadFailedCallback cb)
{
    g_loadFailedCallback = std::move(cb);
}

void setScanProgressCallback(ScanProgressCallback cb)
{
    g_scanProgressCallback = std::move(cb);
}

int scan(const QStringList &roots)
{
    unloadAll();

    QSet<QString> seenFiles;
    const QStringList candidates = gatherCandidateFiles(roots, seenFiles);
    const int total = candidates.size();

    int loaded = 0;
    for (int i = 0; i < total; ++i)
    {
        const QString &path = candidates[i];

        if (g_scanProgressCallback)
            g_scanProgressCallback(i + 1, total, path);

        auto *loader = new QPluginLoader(path);
        if (!loader->load())
        {
            const QString error = loader->errorString();
            if (g_loadFailedCallback) g_loadFailedCallback(path, error);
            delete loader;
            continue;
        }

        QObject *obj = loader->instance();
        auto *driver = qobject_cast<ICameraDriver *>(obj);
        if (!driver)
        {
            if (g_loadFailedCallback) g_loadFailedCallback(path, "Plugin does not implement ICameraDriver");
            loader->unload();
            delete loader;
            continue;
        }

        Entry e;
        e.filePath = path;
        e.cameraIds = driver->enumerate();
        e.instance = driver;
        g_entries.append(e);
        ++loaded;
    }
    return loaded;
}

int scanDefaultRoots()
{
    QStringList roots = defaultRoots();
    roots += g_extraRoots;
    return scan(roots);
}

const QVector<Entry> &entries()
{
    return g_entries;
}

const Entry *findByCamera(const QString &cameraId)
{
    for (const Entry &e : g_entries)
    {
        if (e.cameraIds.contains(cameraId))
            return &e;
    }
    return nullptr;
}

QStringList enumerateCameras()
{
    QStringList ids;
    for (const Entry &e : g_entries)
        ids += e.cameraIds;
    return ids;
}

void unloadAll()
{
    g_entries.clear();
}

}
