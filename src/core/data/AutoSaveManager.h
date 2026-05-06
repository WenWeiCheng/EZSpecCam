#ifndef AUTOSAVEMANAGER_H
#define AUTOSAVEMANAGER_H

#include <QObject>
#include <QString>
#include <QDir>
#include <QStandardPaths>
#include <QFutureWatcher>

#include "DataSaver.h"
#include "interfaces/CameraTypes.h"

class AutoSaveManager : public QObject
{
    Q_OBJECT

public:
    explicit AutoSaveManager(QObject *parent = nullptr);
    ~AutoSaveManager() override;

    void setAutoSaveDirectory(const QString &path);
    QString autoSaveDirectory() const;

    void setAutoSaveEnabled(bool enabled);
    bool isAutoSaveEnabled() const;

    void setImageFormat(ImageFormat format);
    void setFrameSaveFormat(MetaDataSaveFormat format);

    void setSaveOriginal(bool saveOriginal);
    bool saveOriginal() const;
    int currentFrameNumber() const;
    void incrementFrameNumber();
    void resetFrameNumber();

public slots:
    void onFrameReady(const ImageData &frame);

signals:
    void frameAutoSaved(const QString &filePath, bool success);
    void autoSaveError(const QString &error);

private:
    bool ensureAutoSaveDirectory();
    QString createTimestampDirectory();
    QString performAsyncSave(const ImageData &frame, const QString &directory, int frameNumber);
    QByteArray formatToByteArray(ImageFormat format) const;

    DataSaver m_dataSaver;
    QString m_autoSaveDirectory;
    QString m_timestampDirectory;
    bool m_autoSaveEnabled;
    bool m_saveOriginal;
    bool m_timestampCreated;
    int m_frameCounter;
    static const QString DEFAULT_SUBDIR;
    QFutureWatcher<QString> *m_saveWatcher;
};

#endif
