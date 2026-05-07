#include "CaptureController.h"

#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include <iostream>

CaptureController::CaptureController(const CommandLineArgs &args,
                                 ICameraDriver *driver,
                                 volatile sig_atomic_t *signalFlag,
                                 QObject *parent)
    : QObject(parent)
    , m_args(args)
    , m_driver(driver)
    , m_signalFlag(signalFlag)
{
    if (m_driver) {
        connect(m_driver, &ICameraDriver::frameReady,
                this, &CaptureController::onFrameReady);
        connect(m_driver, &ICameraDriver::captureStarted,
                this, &CaptureController::onCaptureStarted);
        connect(m_driver, &ICameraDriver::captureStopped,
                this, &CaptureController::onCaptureStopped);
        connect(m_driver, &ICameraDriver::connectionChanged,
                this, &CaptureController::onConnectionChanged);
        connect(m_driver, &ICameraDriver::errorOccurred,
                this, &CaptureController::onErrorOccurred);
    }
}

CaptureController::~CaptureController()
{
    if (m_driver && m_driver->isConnected()) {
        disconnectCamera();
    }
}

int CaptureController::execute()
{
    qInfo() << "Starting capture workflow...";

    if (!m_args.isValid()) {
        m_lastError = "Invalid command-line arguments";
        qWarning() << "Configuration error:" << m_lastError;
        return 3;
    }

    if (!connectToCamera()) {
        qWarning() << "Failed to connect to camera:" << m_lastError;
        return 1;
    }

    qInfo() << "Connected to camera:" << m_driver->cameraId();

    if (!configureDriver()) {
        disconnectCamera();
        return 3;
    }

    qInfo() << "Driver configured successfully";

    if (!createOutputDirectory()) {
        qWarning() << "Failed to create output directory:" << m_lastError;
        return 3;
    }

    qInfo() << "Output directory:" << m_outputDir;

    m_capturedFrameCount = 0;
    m_captureStarted = false;
    m_captureStopped = false;
    m_errorOccurred = false;

    qInfo() << "Starting capture...";

    if (!m_driver->startCapture(m_args.captureCount)) {
        m_lastError = "Failed to start capture";
        qWarning() << m_lastError;
        disconnectCamera();
        return 2;
    }

    if (m_args.captureCount == 0) {
        qInfo() << "Continuous capture mode (press Ctrl+C to stop)";
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(false);
        timer.start(100);

        QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
            if (shouldStop() || m_errorOccurred) {
                loop.quit();
            }
        });

        QObject::connect(m_driver, &ICameraDriver::frameReady, &loop, &QEventLoop::quit);
        QObject::connect(m_driver, &ICameraDriver::captureStopped, &loop, &QEventLoop::quit);

        loop.exec();
        timer.stop();
    } else {
        QEventLoop loop;
        m_captureLoop = &loop;

        QObject::connect(m_driver, &ICameraDriver::captureStopped, &loop, &QEventLoop::quit);
        QObject::connect(m_driver, &ICameraDriver::errorOccurred, &loop, &QEventLoop::quit);

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        timeoutTimer.start(30000);

        QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
            qWarning() << "Capture timeout";
            loop.quit();
        });

        loop.exec();
        m_captureLoop = nullptr;
    }

    qInfo() << "Stopping capture...";
    m_driver->stopCapture(5000);

    disconnectCamera();

    if (m_errorOccurred) {
        qWarning() << "Capture failed with error:" << m_lastError;
        return 2;
    }

    qInfo() << "Capture completed successfully." << m_capturedFrameCount << "frames saved to" << m_outputDir;
    return 0;
}

QString CaptureController::lastError() const
{
    return m_lastError;
}

int CaptureController::capturedFrameCount() const
{
    return m_capturedFrameCount;
}

void CaptureController::onFrameReady(const QSharedPointer<QImage> &image,
                                    quint64 timestamp,
                                    int frameNumber,
                                    const QString &cameraId)
{
    Q_UNUSED(cameraId);

    if (!image || image->isNull()) {
        qWarning() << "Received null frame, skipping";
        return;
    }

    ImageData frameData;
    frameData.image = *image;
    frameData.timestamp = timestamp;
    frameData.frameNumber = frameNumber;
    frameData.cameraId = m_driver->cameraId();

    QVariantMap params;
    params["exposure"] = m_args.exposure;
    params["gain"] = m_args.gain;
    frameData.parameters = params;

    QString filePath = QString("%1/img_%2.tiff")
        .arg(m_outputDir)
        .arg(frameNumber, 12, 10, QChar('0'));
    if (frameData.image.save(filePath, "TIFF")) {
        m_capturedFrameCount++;
    }
}

void CaptureController::onCaptureStarted(const QString &cameraId)
{
    Q_UNUSED(cameraId);
    m_captureStarted = true;
    qInfo() << "Capture started";
}

void CaptureController::onCaptureStopped(const QString &cameraId)
{
    Q_UNUSED(cameraId);
    m_captureStopped = true;
    qInfo() << "Capture stopped";

    if (m_captureLoop) {
        m_captureLoop->quit();
    }
}

void CaptureController::onConnectionChanged(bool connected, const QString &cameraId)
{
    Q_UNUSED(cameraId);
    m_connected = connected;
    m_connectionChanged = true;
    qInfo() << "Connection changed:" << (connected ? "connected" : "disconnected");
}

void CaptureController::onErrorOccurred(const CameraError &error)
{
    m_errorOccurred = true;
    m_lastError = error.description;
    qWarning() << "Driver error:" << error.description;

    if (m_captureLoop) {
        m_captureLoop->quit();
    }
}

bool CaptureController::waitForSignal(const char *signalName, int timeoutMs)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(timeoutMs);

    QObject::connect(m_driver, &ICameraDriver::connectionChanged, &loop, &QEventLoop::quit);
    QObject::connect(m_driver, &ICameraDriver::errorOccurred, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    loop.exec();

    bool signalReceived = !timer.isActive();
    timer.stop();

    return signalReceived;
}

bool CaptureController::createOutputDirectory()
{
    QString outputDir = m_args.outputDir.isEmpty() ? "data" : m_args.outputDir;

    QDir dir(outputDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            m_lastError = "Failed to create output directory: " + outputDir;
            return false;
        }
    }

    QString timestamp = generateTimestamp();
    m_outputDir = dir.absoluteFilePath(timestamp);

    if (!dir.mkpath(timestamp)) {
        m_lastError = "Failed to create timestamp directory: " + m_outputDir;
        return false;
    }

    return true;
}

bool CaptureController::configureDriver()
{
    m_driver->setParameter("exposure", m_args.exposure);
    m_driver->setParameter("gain", m_args.gain);

    if (!m_driver->validateParameters()) {
        m_lastError = "Invalid parameters";
        return false;
    }

    if (!m_driver->commitParameters()) {
        m_lastError = "Failed to commit parameters";
        return false;
    }

    return true;
}

bool CaptureController::connectToCamera()
{
    if (!m_driver) {
        m_lastError = "Driver not initialized";
        return false;
    }

    m_errorOccurred = false;
    m_connectionChanged = false;
    m_connected = false;

    if (!m_driver->connectToCamera(m_args.cameraId)) {
        m_lastError = "Failed to initiate camera connection";
        return false;
    }

    if (!waitForSignal("connectionChanged", 30000)) {
        if (m_errorOccurred) {
            return false;
        }
        m_lastError = "Connection timeout";
        return false;
    }

    if (m_errorOccurred) {
        return false;
    }

    return m_connected;
}

void CaptureController::disconnectCamera()
{
    if (m_driver && m_driver->isConnected()) {
        qInfo() << "Disconnecting from camera...";
        m_driver->disconnectCamera();
        waitForSignal("connectionChanged", 5000);
    }
}

bool CaptureController::shouldStop() const
{
    bool signalReceived = m_signalFlag != nullptr && *m_signalFlag != 0;
    return signalReceived || m_errorOccurred;
}

QString CaptureController::generateTimestamp() const
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss");
}