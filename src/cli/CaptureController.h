#ifndef CAPTURECONTROLLER_H
#define CAPTURECONTROLLER_H

#include <QObject>
#include <QString>
#include <QEventLoop>
#include <QTimer>
#include <QDir>
#include <QDateTime>
#include <QSharedPointer>
#include <QImage>
#include <csignal>

#include "CommandLineParser.h"
#include "core/interfaces/ICameraDriver.h"
#include "core/interfaces/CameraTypes.h"
#include "core/data/AutoSaveManager.h"

class CaptureController : public QObject
{
    Q_OBJECT

public:
    explicit CaptureController(const CommandLineArgs &args,
                                ICameraDriver *driver,
                                volatile sig_atomic_t *signalFlag = nullptr,
                                QObject *parent = nullptr);

    ~CaptureController() override;

    int execute();

    QString lastError() const;
    int capturedFrameCount() const;

private slots:
    void onFrameReady(const QSharedPointer<QImage> &image,
                      quint64 timestamp,
                      int frameNumber,
                      const QString &cameraId);

    void onCaptureStarted(const QString &cameraId);
    void onCaptureStopped(const QString &cameraId);
    void onConnectionChanged(bool connected, const QString &cameraId);
    void onErrorOccurred(const CameraError &error);
    void onFrameAutoSaved(const QString &filePath, bool success);

private:
    bool waitForSignal(const char *signalName, int timeoutMs);

    bool createOutputDirectory();
    bool configureDriver();
    bool connectToCamera();
    void disconnectCamera();

    bool shouldStop() const;
    QString generateTimestamp() const;

    CommandLineArgs m_args;
    ICameraDriver *m_driver = nullptr;
    AutoSaveManager *m_autoSaveManager = nullptr;
    volatile sig_atomic_t *m_signalFlag = nullptr;

    QString m_lastError;
    QString m_outputDir;
    int m_capturedFrameCount = 0;

    bool m_captureStarted = false;
    bool m_captureStopped = false;
    bool m_connectionChanged = false;
    bool m_errorOccurred = false;
    bool m_connected = false;

    QEventLoop *m_captureLoop = nullptr;
};

#endif