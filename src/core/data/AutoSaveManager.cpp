#include "AutoSaveManager.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QtConcurrent/QtConcurrentRun>

const QString AutoSaveManager::DEFAULT_SUBDIR = "EZSpecCamData";

AutoSaveManager::AutoSaveManager(QObject *parent)
    : QObject(parent)
    , m_autoSaveDirectory(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                         + QDir::separator() + DEFAULT_SUBDIR)
    , m_autoSaveEnabled(false)
    , m_saveOriginal(false)
    , m_timestampCreated(false)
    , m_frameCounter(0)
    , m_saveWatcher(new QFutureWatcher<QString>(this))
{
    m_saveOriginal = false;
    connect(m_saveWatcher, &QFutureWatcher<QString>::finished,
            this, [this]() {
                QString result = m_saveWatcher->result();
                if (!result.isEmpty()) {
                    emit frameAutoSaved(result, true);
                } else {
                    emit autoSaveError("Failed to save frame asynchronously");
                    emit frameAutoSaved(QString(), false);
                }
            });
}

AutoSaveManager::~AutoSaveManager()
{
    if (m_saveWatcher->isRunning()) {
        m_saveWatcher->waitForFinished();
    }
}

void AutoSaveManager::setAutoSaveDirectory(const QString &path)
{
    if (m_autoSaveDirectory != path) {
        m_autoSaveDirectory = path;
        m_timestampCreated = false;
        m_timestampDirectory.clear();
        m_frameCounter = 0;
    }
}

QString AutoSaveManager::autoSaveDirectory() const
{
    return m_autoSaveDirectory;
}

void AutoSaveManager::setAutoSaveEnabled(bool enabled)
{
    if (m_autoSaveEnabled != enabled) {
        m_autoSaveEnabled = enabled;

        if (!enabled) {
            m_timestampCreated = false;
            m_timestampDirectory.clear();
            m_frameCounter = 0;
        }
    }
}

bool AutoSaveManager::isAutoSaveEnabled() const
{
    return m_autoSaveEnabled;
}

void AutoSaveManager::setImageFormat(ImageFormat format)
{
    ImageSaveOptions opts = m_dataSaver.saveOptions();
    opts.format = format;
    m_dataSaver.setSaveOptions(opts);
}

void AutoSaveManager::setFrameSaveFormat(MetaDataSaveFormat format)
{
    FrameSaveOptions opts = m_dataSaver.frameSaveOptions();
    opts.frameFormat = format;
    m_dataSaver.setFrameSaveOptions(opts);
}

void AutoSaveManager::setSaveOriginal(bool saveOriginal)
{
    m_saveOriginal = saveOriginal;
}

bool AutoSaveManager::saveOriginal() const
{
    return m_saveOriginal;
}

int AutoSaveManager::currentFrameNumber() const
{
    return m_frameCounter;
}

void AutoSaveManager::incrementFrameNumber()
{
    ++m_frameCounter;
}

void AutoSaveManager::resetFrameNumber()
{
    m_frameCounter = 0;
}

void AutoSaveManager::onFrameReady(const ImageData &frame)
{
    if (!m_autoSaveEnabled) {
        return;
    }

    if (!frame.isValid()) {
        emit autoSaveError("Invalid frame data - cannot save");
        emit frameAutoSaved(QString(), false);
        return;
    }

    if (!ensureAutoSaveDirectory()) {
        emit autoSaveError("Failed to create auto-save directory");
        emit frameAutoSaved(QString(), false);
        return;
    }

    ImageData frameToSave = frame;
    if (m_saveOriginal && frame.hasOriginal()) {
        frameToSave.image = frame.originalImage;
    }

    int frameNumber = ++m_frameCounter;

    QFuture<QString> future = QtConcurrent::run([this, frameToSave, frameNumber]() {
        return performAsyncSave(frameToSave, m_timestampDirectory, frameNumber);
    });

    m_saveWatcher->setFuture(future);
}

bool AutoSaveManager::ensureAutoSaveDirectory()
{
    QDir baseDir(m_autoSaveDirectory);

    if (!baseDir.exists()) {
        if (!baseDir.mkpath(".")) {
            qWarning() << "AutoSaveManager: Failed to create base directory:" << m_autoSaveDirectory;
            return false;
        }
    }

    if (!m_timestampCreated) {
        QString timestampDir = createTimestampDirectory();
        if (timestampDir.isEmpty()) {
            return false;
        }
        m_timestampDirectory = timestampDir;
        m_timestampCreated = true;
    }

    return true;
}

QString AutoSaveManager::createTimestampDirectory()
{
    QDateTime now = QDateTime::currentDateTime();
    QString timestamp = now.toString("yyyy-MM-dd-hh-mm-ss");

    QString fullPath = m_autoSaveDirectory + QDir::separator() + timestamp;

    QDir dir(fullPath);
    if (dir.exists()) {
        return fullPath;
    }

    if (dir.mkpath(".")) {
        return fullPath;
    }

    qWarning() << "AutoSaveManager: Failed to create timestamp directory:" << fullPath;
    return QString();
}

QString AutoSaveManager::performAsyncSave(const ImageData &frame, const QString &directory, int frameNumber)
{
    DataSaver threadLocalSaver;

    threadLocalSaver.setSaveOptions(m_dataSaver.saveOptions());
    threadLocalSaver.setFrameSaveOptions(m_dataSaver.frameSaveOptions());

    if (threadLocalSaver.saveFrame(frame, directory, frameNumber)) {
        QString fileName = QString("img_%1.%2")
            .arg(frameNumber, 12, 10, QChar('0'))
            .arg(QString::fromLatin1(formatToByteArray(m_dataSaver.saveOptions().format).toLower()));
        return directory + QDir::separator() + fileName;
    }

    return QString();
}

QByteArray AutoSaveManager::formatToByteArray(ImageFormat format) const
{
    switch (format) {
    case ImageFormat::JPEG:
        return "jpg";
    case ImageFormat::TIFF:
    default:
        return "tiff";
    }
}
