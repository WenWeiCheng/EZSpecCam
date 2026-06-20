#include "HeadlessController.h"

#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QSharedPointer>
#include <QTextStream>

#include "ICameraDriver.h"
#include "CameraTypes.h"
#include "MessageHandler.h"
#include "PluginLoader.h"
#include "WaitStabilizer.h"
#include "formats/FrameWriter.h"

namespace app
{

namespace
{

void listCameras()
{
    qInfo() << "Available cameras:";
    bool found = false;
    for (const auto &e : app::plugins::entries())
    {
        for (const QString &id : e.cameraIds)
        {
            qInfo().noquote() << "  " << id << " (driver: " << e.filePath << ")";
            found = true;
        }
    }
    if (!found) qInfo() << "  (none)";
}

void listParameters(ICameraDriver *driver)
{
    qInfo() << "Parameters for" << driver->cameraId() << ":";
    for (const QString &name : driver->parameterNames())
    {
        ParameterDefinition def = driver->parameter(name);
        QVariant val = driver->parameterValue(name);
        QString flags;
        if (def.isReadOnly)  flags += " [readonly]";
        if (def.isExtrinsic) flags += " [extrinsic]";
        if (def.isDynamic)   flags += " [dynamic]";
        qInfo().noquote() << QString("  %1 = %2 (%3)%4")
            .arg(name, -28)
            .arg(val.toString(), -16)
            .arg(def.displayName)
            .arg(flags);
    }
}

int captureFrames(ICameraDriver *driver, int frameCount,
                  const QString &outputDir, const QString &format,
                  const QString &prefix, const QString &suffix)
{
    if (frameCount <= 0) { qCritical() << "Frame count must be > 0"; return -1; }

    QEventLoop loop;
    int captured = 0;
    int errors = 0;

    QObject::connect(driver, &ICameraDriver::frameReady,
        [&](const QSharedPointer<QImage> &image, quint64 ts, int frameNumber,
            const QString &cameraId, const QVariantMap &parameters)
        {
            if (captured >= frameCount) return;
            ImageData frame;
            frame.image = *image;
            frame.originalImage = *image;
            frame.timestamp = ts;
            frame.frameNumber = frameNumber;
            frame.cameraId = cameraId;
            frame.parameters = parameters;
            const QString ext = app::formats::extensionForCliFormat(format);
            const QString filePath = app::formats::generateFilename(outputDir, prefix, suffix, ext);
            if (!app::formats::saveFrame(frame, filePath)) errors++;
            captured++;
            qInfo() << "Frame" << captured << "/" << frameCount;
            if (captured >= frameCount) loop.quit();
        });

    QObject::connect(driver, &ICameraDriver::captureStopped,
        [&](const QString &) {
            if (captured < frameCount) {
                qWarning() << "Capture stopped unexpectedly";
                loop.quit();
            }
        });

    QObject::connect(driver, &ICameraDriver::errorOccurred,
        [&](const CameraError &err) {
            qWarning().noquote() << "Camera error:" << err.description;
            errors++;
        });

    qInfo() << "Starting capture:" << frameCount << "frames...";
    if (!driver->startCapture(frameCount))
    {
        qCritical() << "Failed to start capture";
        return -1;
    }
    loop.exec();
    driver->stopCapture();
    qInfo() << "Capture complete:" << captured << "frames," << errors << "errors";
    return captured;
}

int runSequence(ICameraDriver *driver, const HeadlessOptions &opts,
                const QVector<SequenceStep> &steps)
{
    qInfo() << "Running sequence:" << steps.size() << "steps";
    for (int i = 0; i < steps.size(); ++i)
    {
        const SequenceStep &step = steps[i];
        qInfo() << "";
        qInfo() << QString("--- Step %1/%2 ---").arg(i + 1).arg(steps.size());
        switch (step.type)
        {
        case SequenceStep::Configure:
            for (auto it = step.parameters.constBegin(); it != step.parameters.constEnd(); ++it)
            {
                qInfo().noquote() << "  set" << it.key() << "=" << it.value().toString();
                if (!driver->setParameter(it.key(), it.value()))
                    qWarning().noquote() << "  Failed to set" << it.key();
            }
            if (!driver->commitParameters())
                qWarning() << "  Failed to commit parameters";
            break;
        case SequenceStep::Capture:
        {
            QString outDir = step.outputDir.isEmpty() ? opts.outputDir : step.outputDir;
            QString fmt    = step.format.isEmpty()    ? opts.format    : step.format;
            QString pfx    = step.prefix.isEmpty()    ? opts.prefix    : step.prefix;
            QString sfx    = step.suffix.isEmpty()    ? opts.suffix    : step.suffix;
            if (captureFrames(driver, step.frames, outDir, fmt, pfx, sfx) < 0)
                return -1;
            break;
        }
        case SequenceStep::WaitStable:
            if (!waitForStable(driver, step.stableParam, step.stableTarget,
                               step.stableTolerance, step.stableWindowSec,
                               step.stableTimeoutSec))
                return -1;
            break;
        }
    }
    return 0;
}

}

int run(const HeadlessOptions &opts)
{
    int loaded = app::plugins::scanDefaultRoots();
    if (loaded == 0) { qCritical() << "No camera drivers found"; return 1; }

    if (opts.listCameras) { listCameras(); return 0; }

    if (opts.cameraId.isEmpty()) {
        qCritical() << "No camera specified. Use --camera <id> or --list.";
        return 1;
    }

    const app::plugins::Entry *entry = app::plugins::findByCamera(opts.cameraId);
    if (!entry) { qCritical().noquote() << "Camera not found:" << opts.cameraId; return 1; }
    ICameraDriver *driver = entry->instance;
    if (!driver) { qCritical() << "Driver instance null"; return 1; }

    qInfo().noquote() << "Connecting to" << opts.cameraId << "...";
    if (!driver->connectToCamera(opts.cameraId)) {
        qCritical() << "Failed to connect to" << opts.cameraId;
        return 1;
    }
    qInfo() << "Connected.";

    if (opts.listParams) { listParameters(driver); driver->disconnectCamera(); return 0; }

    for (auto it = opts.setParameters.constBegin(); it != opts.setParameters.constEnd(); ++it)
    {
        qInfo().noquote() << "Setting" << it.key() << "=" << it.value().toString();
        if (!driver->setParameter(it.key(), it.value()))
            qWarning().noquote() << "  Warning: setParameter returned false for" << it.key();
    }
    if (!opts.setParameters.isEmpty() && !driver->commitParameters())
        qWarning() << "Warning: commitParameters returned false";

    int rc = 0;
    if (!opts.sequence.isEmpty())
        rc = runSequence(driver, opts, opts.sequence);
    else
        rc = captureFrames(driver, opts.frames, opts.outputDir, opts.format, opts.prefix, opts.suffix);

    driver->disconnectCamera();
    return (rc >= 0) ? 0 : 1;
}

}
