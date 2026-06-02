#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QPluginLoader>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QDateTime>
#include <QTimer>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QDebug>

#include "core/ICameraDriver.h"
#include "core/CameraTypes.h"
#include "SequenceRunner.h"

// ============================================================================
// Save helpers
// ============================================================================

static QString generateFilename(const QString &outputDir, const QString &format,
                                 const QString &prefix, const QString &suffix)
{
    QString ext = format.toLower();
    if (ext.isEmpty())
        ext = QStringLiteral("tiff");

    QString name;
    if (!prefix.isEmpty())
        name += prefix + QStringLiteral("_");
    name += QStringLiteral("img_");
    name += QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz"));
    if (!suffix.isEmpty())
        name += QStringLiteral("_") + suffix;
    name += QStringLiteral(".") + ext;

    QDir dir(outputDir.isEmpty() ? QStringLiteral(".") : outputDir);
    return dir.absoluteFilePath(name);
}

static bool saveMetadataJson(const QString &imgPath, const ImageData &frame)
{
    QFileInfo fi(imgPath);
    QString metaPath = fi.absoluteDir().absolutePath() + QStringLiteral("/")
                     + fi.completeBaseName() + QStringLiteral("_metadata.json");

    QJsonObject root;
    root[QStringLiteral("cameraId")] = frame.cameraId;
    root[QStringLiteral("timestamp")] = static_cast<qint64>(frame.timestamp);
    root[QStringLiteral("frameNumber")] = frame.frameNumber;

    QJsonObject params;
    for (auto it = frame.parameters.constBegin(); it != frame.parameters.constEnd(); ++it)
        params[it.key()] = QJsonValue::fromVariant(it.value());
    root[QStringLiteral("parameters")] = params;

    QFile file(metaPath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

static bool saveAsTiff(const ImageData &frame, const QString &filePath, bool withMetadata)
{
    QFileInfo fi(filePath);
    QDir().mkpath(fi.absolutePath());

    if (!frame.image.save(filePath, "TIFF"))
    {
        qWarning().noquote() << "Failed to save TIFF:" << filePath;
        return false;
    }

    if (withMetadata)
        saveMetadataJson(filePath, frame);

    qInfo().noquote() << "  Saved:" << QFileInfo(filePath).fileName();
    return true;
}

static bool saveAsCsv(const ImageData &frame, const QString &filePath, bool withMetadata)
{
    QFileInfo fi(filePath);
    QDir().mkpath(fi.absolutePath());

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning().noquote() << "Failed to open CSV:" << filePath;
        return false;
    }

    QTextStream out(&file);

    if (frame.image.height() == 1 && !frame.spectrum.isEmpty())
    {
        out << "Index,Counts\n";
        for (int i = 0; i < frame.spectrum.size(); ++i)
            out << i << "," << frame.spectrum[i] << "\n";
    }
    else
    {
        out << "Row,Col,Value\n";
        for (int y = 0; y < frame.image.height(); ++y)
        {
            const quint16 *row = reinterpret_cast<const quint16 *>(frame.image.constScanLine(y));
            for (int x = 0; x < frame.image.width(); ++x)
                out << y << "," << x << "," << row[x] << "\n";
        }
    }

    file.close();

    if (withMetadata)
        saveMetadataJson(filePath, frame);

    qInfo().noquote() << "  Saved:" << QFileInfo(filePath).fileName();
    return true;
}

static bool saveFrame(const ImageData &frame, const QString &filePath,
                       const QString &format, bool withMetadata)
{
    if (format == QStringLiteral("csv"))
        return saveAsCsv(frame, filePath, withMetadata);
    return saveAsTiff(frame, filePath, withMetadata);
}

// ============================================================================
// Plugin loading
// ============================================================================
// Loader objects intentionally leaked for simplicity — drivers must outlive
// the application. A production version would manage ownership explicitly.

static QVector<ICameraDriver *> loadAllDrivers()
{
    QVector<ICameraDriver *> drivers;

    QStringList searchPaths;
    searchPaths << QCoreApplication::applicationDirPath() + QStringLiteral("/plugins/drivers");
    searchPaths << QCoreApplication::applicationDirPath() + QStringLiteral("/../plugins/drivers");

    for (const QString &dirPath : searchPaths)
    {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;

        for (const QFileInfo &fi : dir.entryInfoList(QDir::Files))
        {
            QString path = fi.absoluteFilePath();
            if (!path.endsWith(QStringLiteral(".dll"), Qt::CaseInsensitive))
                continue;

            auto *loader = new QPluginLoader(path);
            QObject *instance = loader->instance();
            if (!instance)
            {
                delete loader;
                continue;
            }

            auto *driver = qobject_cast<ICameraDriver *>(instance);
            if (driver)
                drivers.append(driver);
            else
            {
                loader->unload();
                delete loader;
            }
        }
    }

    return drivers;
}

// ============================================================================
// Camera helpers
// ============================================================================

static ICameraDriver *findCamera(const QVector<ICameraDriver *> &drivers,
                                  const QString &cameraId)
{
    for (auto *d : drivers)
    {
        if (d->enumerate().contains(cameraId))
            return d;
    }
    return nullptr;
}

static void listCameras(const QVector<ICameraDriver *> &drivers)
{
    qInfo() << "Available cameras:";
    bool found = false;
    for (auto *d : drivers)
    {
        for (const QString &id : d->enumerate())
        {
            qInfo().noquote() << "  " << id << " (driver: " << d->driverVersion() << ")";
            found = true;
        }
    }
    if (!found)
        qInfo() << "  (none)";
}

static void listParameters(ICameraDriver *driver)
{
    qInfo() << "Parameters for" << driver->cameraId() << ":";
    for (const QString &name : driver->parameterNames())
    {
        ParameterDefinition def = driver->parameter(name);
        QVariant val = driver->parameterValue(name);

        QString flags;
        if (def.isReadOnly)  flags += QStringLiteral(" [readonly]");
        if (def.isExtrinsic) flags += QStringLiteral(" [extrinsic]");
        if (def.isDynamic)   flags += QStringLiteral(" [dynamic]");

        qInfo().noquote() << QString("  %1 = %2 (%3)%4")
            .arg(name, -28)
            .arg(val.toString(), -16)
            .arg(def.displayName)
            .arg(flags);
    }
}

// ============================================================================
// Capture
// ============================================================================

static int captureFrames(ICameraDriver *driver, int frameCount,
                          const QString &outputDir, const QString &format,
                          const QString &prefix, const QString &suffix,
                          bool saveMetadata)
{
    if (frameCount <= 0)
    {
        qCritical() << "Frame count must be > 0";
        return -1;
    }

    QEventLoop loop;
    int captured = 0;
    int errors = 0;

    QMetaObject::Connection frameConn = QObject::connect(
        driver, &ICameraDriver::frameReady,
        [&](const QSharedPointer<QImage> &image, quint64 timestamp,
            int frameNumber, const QString &cameraId,
            const QVariantMap &parameters)
        {
            if (captured >= frameCount)
                return;

            ImageData frame;
            frame.image = *image;
            frame.timestamp = timestamp;
            frame.frameNumber = frameNumber;
            frame.cameraId = cameraId;
            frame.parameters = parameters;

            QString filePath = generateFilename(outputDir, format, prefix, suffix);

            if (!saveFrame(frame, filePath, format, saveMetadata))
                errors++;

            captured++;
            qInfo() << "Frame" << captured << "/" << frameCount;

            if (captured >= frameCount)
                loop.quit();
        });

    QMetaObject::Connection stoppedConn = QObject::connect(
        driver, &ICameraDriver::captureStopped,
        [&](const QString &) {
            if (captured < frameCount)
            {
                qWarning() << "Capture stopped unexpectedly at frame" << captured;
                loop.quit();
            }
        });

    QMetaObject::Connection errConn = QObject::connect(
        driver, &ICameraDriver::errorOccurred,
        [&](const CameraError &error) {
            qWarning().noquote() << "Camera error:" << error.description;
            errors++;
        });

    qInfo() << "Starting capture:" << frameCount << "frames...";

    if (!driver->startCapture(frameCount))
    {
        qCritical() << "Failed to start capture";
        QObject::disconnect(frameConn);
        QObject::disconnect(stoppedConn);
        QObject::disconnect(errConn);
        return -1;
    }

    loop.exec();

    driver->stopCapture();

    QObject::disconnect(frameConn);
    QObject::disconnect(stoppedConn);
    QObject::disconnect(errConn);

    qInfo() << "Capture complete:" << captured << "frames," << errors << "errors";
    return captured;
}

// ============================================================================
// Wait for stability
// ============================================================================

static bool waitForStable(ICameraDriver *driver, const QString &paramName,
                           double target, double tolerance,
                           double windowSec, double timeoutSec)
{
    qInfo().noquote() << QString("Waiting for %1 to stabilize at %2 (+-%3) "
                                  "within %4s window (timeout: %5s)...")
        .arg(paramName).arg(target).arg(tolerance)
        .arg(windowSec).arg(timeoutSec);

    QElapsedTimer totalTimer;
    totalTimer.start();

    struct Sample { qint64 ms; double value; };
    QVector<Sample> samples;

    QTimer pollTimer;
    pollTimer.setInterval(500);

    QEventLoop loop;
    int exitCode = -1;

    QTextStream statusOut(stdout);

    QObject::connect(&pollTimer, &QTimer::timeout, [&]() {
        QVariant val = driver->parameterValue(paramName);
        if (!val.isValid())
        {
            statusOut << '\n';
            statusOut.flush();
            qWarning() << "  Cannot read parameter:" << paramName;
            exitCode = -1;
            loop.quit();
            return;
        }

        double current = val.toDouble();
        qint64 elapsed = totalTimer.elapsed();
        samples.append({elapsed, current});

        while (!samples.isEmpty() && (elapsed - samples.first().ms) > windowSec * 1000)
            samples.removeFirst();

        // In-place status line: overwrite same line with \r (no newline)
        {
            QString status = QStringLiteral("  %1 = %2 (target: %3, delta=%4, elapsed: %5s)")
                .arg(paramName, -28)
                .arg(current, 8, 'f', 2)
                .arg(target, 8, 'f', 2)
                .arg(qAbs(current - target), 0, 'f', 3)
                .arg(elapsed / 1000.0, 0, 'f', 1);
            // Right-pad to clear any longer previous line
            status = status.leftJustified(79, QLatin1Char(' '));
            statusOut << '\r' << status;
            statusOut.flush();
        }

        if (samples.size() >= 3)
        {
            bool stable = true;
            for (const auto &s : samples)
            {
                if (qAbs(s.value - target) > tolerance)
                {
                    stable = false;
                    break;
                }
            }
            if (stable)
            {
                double windowMs = windowSec * 1000;
                double actualWindow = elapsed - samples.first().ms;
                if (actualWindow >= windowMs * 0.9)
                {
                    statusOut << '\n';
                    statusOut.flush();
                    qInfo() << "  Stable! All readings within" << tolerance
                            << "for " << windowSec << "s";
                    exitCode = 0;
                    loop.quit();
                    return;
                }
            }
        }

        if (elapsed > timeoutSec * 1000)
        {
            statusOut << '\n';
            statusOut.flush();
            qWarning() << "  Timeout! Stability not reached within" << timeoutSec << "s";
            exitCode = -1;
            loop.quit();
        }
    });

    pollTimer.start();
    loop.exec();
    pollTimer.stop();

    return exitCode == 0;
}

// ============================================================================
// Sequence execution
// ============================================================================

static int runSequence(ICameraDriver *driver, const SequenceRunner &seq)
{
    const auto &steps = seq.steps();
    qInfo() << "Running sequence:" << steps.size() << "steps";

    for (int i = 0; i < steps.size(); ++i)
    {
        const SequenceStep &step = steps[i];
        qInfo() << "";
        qInfo() << QString("--- Step %1/%2 ---").arg(i + 1).arg(steps.size());

        switch (step.type)
        {
        case SequenceStep::Configure:
            qInfo() << "Configuring parameters...";
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
            QString outDir = step.outputDir.isEmpty() ? seq.defaultOutputDir : step.outputDir;
            QString fmt = step.format.isEmpty() ? seq.defaultFormat : step.format;
            QString pfx = step.prefix.isEmpty() ? seq.defaultPrefix : step.prefix;
            QString sfx = step.suffix.isEmpty() ? seq.defaultSuffix : step.suffix;
            bool meta = step.saveMetadata;

            int captured = captureFrames(driver, step.frames, outDir, fmt, pfx, sfx, meta);
            if (captured < 0)
            {
                qCritical() << "Capture failed, aborting sequence";
                return -1;
            }
            break;
        }

        case SequenceStep::WaitStable:
            if (!waitForStable(driver, step.stableParam, step.stableTarget,
                               step.stableTolerance, step.stableWindowSec,
                               step.stableTimeoutSec))
            {
                qCritical() << "Stability wait failed, aborting sequence";
                return -1;
            }
            break;
        }
    }

    qInfo() << "";
    qInfo() << "Sequence complete.";
    return 0;
}

// ============================================================================
// Main
// ============================================================================

static void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    QTextStream out(type == QtCriticalMsg || type == QtFatalMsg ? stderr : stdout);
    out << msg << "\n";
    out.flush();
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(messageHandler);

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("EZSpecCamCli"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("EZSpecCam CLI — Headless camera control"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption listOpt(QStringLiteral("list"), QStringLiteral("List available cameras"));
    parser.addOption(listOpt);

    QCommandLineOption cameraOpt(QStringLiteral("camera"), QStringLiteral("Camera ID to connect to"), QStringLiteral("id"));
    parser.addOption(cameraOpt);

    QCommandLineOption listParamsOpt(QStringLiteral("list-params"), QStringLiteral("List all camera parameters"));
    parser.addOption(listParamsOpt);

    QCommandLineOption setOpt(QStringLiteral("set"), QStringLiteral("Set parameter (name=value)"), QStringLiteral("param"));
    parser.addOption(setOpt);

    QCommandLineOption framesOpt(QStringLiteral("frames"), QStringLiteral("Number of frames to capture"), QStringLiteral("n"), QStringLiteral("1"));
    parser.addOption(framesOpt);

    QCommandLineOption outputOpt(QStringLiteral("output"), QStringLiteral("Output directory"), QStringLiteral("dir"), QStringLiteral("."));
    parser.addOption(outputOpt);

    QCommandLineOption formatOpt(QStringLiteral("format"), QStringLiteral("Output format (tiff|csv)"), QStringLiteral("fmt"), QStringLiteral("tiff"));
    parser.addOption(formatOpt);

    QCommandLineOption prefixOpt(QStringLiteral("prefix"), QStringLiteral("Filename prefix"), QStringLiteral("str"), QString());
    parser.addOption(prefixOpt);

    QCommandLineOption suffixOpt(QStringLiteral("suffix"), QStringLiteral("Filename suffix"), QStringLiteral("str"), QString());
    parser.addOption(suffixOpt);

    QCommandLineOption noMetaOpt(QStringLiteral("no-metadata"), QStringLiteral("Do not save metadata JSON"));
    parser.addOption(noMetaOpt);

    QCommandLineOption sequenceOpt(QStringLiteral("sequence"), QStringLiteral("Event sequence JSON file"), QStringLiteral("file"));
    parser.addOption(sequenceOpt);

    parser.process(app);

    // --- Load drivers ---
    QVector<ICameraDriver *> drivers = loadAllDrivers();
    if (drivers.isEmpty())
    {
        qCritical() << "No camera drivers found. Ensure plugins are in plugins/drivers/";
        return 1;
    }

    // --- --list ---
    if (parser.isSet(listOpt))
    {
        listCameras(drivers);
        return 0;
    }

    // --- Need a camera from here on ---
    if (!parser.isSet(cameraOpt))
    {
        qCritical() << "No camera specified. Use --camera <id> or --list to see available cameras.";
        return 1;
    }

    QString cameraId = parser.value(cameraOpt);
    ICameraDriver *driver = findCamera(drivers, cameraId);
    if (!driver)
    {
        qCritical().noquote() << "Camera not found:" << cameraId;
        return 1;
    }

    qInfo().noquote() << "Connecting to" << cameraId << "...";
    if (!driver->connectToCamera(cameraId))
    {
        qCritical() << "Failed to connect to" << cameraId;
        return 1;
    }
    qInfo() << "Connected.";

    // --- --list-params ---
    if (parser.isSet(listParamsOpt))
    {
        listParameters(driver);
        driver->disconnectCamera();
        return 0;
    }

    // --- --set parameters ---
    QStringList setValues = parser.values(setOpt);
    for (const QString &sv : setValues)
    {
        int eqIdx = sv.indexOf('=');
        if (eqIdx < 0)
        {
            qWarning().noquote() << "Invalid --set format:" << sv << "(use name=value)";
            continue;
        }
        QString name = sv.left(eqIdx).trimmed();
        QString valueStr = sv.mid(eqIdx + 1).trimmed();

        QVariant value;
        bool ok = false;
        double d = valueStr.toDouble(&ok);
        if (ok)
        {
            if (valueStr.contains('.'))
                value = d;
            else
            {
                qint64 i = valueStr.toLongLong(&ok);
                value = ok ? QVariant(i) : QVariant(d);
            }
        }
        else
        {
            value = valueStr;
        }

        qInfo().noquote() << "Setting" << name << "=" << value.toString();
        if (!driver->setParameter(name, value))
            qWarning().noquote() << "  Warning: setParameter returned false for" << name;
    }

    if (!setValues.isEmpty() && !driver->commitParameters())
        qWarning() << "Warning: commitParameters returned false";

    // --- --sequence ---
    if (parser.isSet(sequenceOpt))
    {
        SequenceRunner seq;
        if (!seq.loadFromFile(parser.value(sequenceOpt)))
        {
            qCritical().noquote() << "Failed to load sequence:" << seq.errorString();
            driver->disconnectCamera();
            return 1;
        }

        if (seq.defaultOutputDir.isEmpty()) seq.defaultOutputDir = parser.value(outputOpt);
        if (seq.defaultFormat.isEmpty())   seq.defaultFormat = parser.value(formatOpt);
        if (seq.defaultPrefix.isEmpty())   seq.defaultPrefix = parser.value(prefixOpt);
        if (seq.defaultSuffix.isEmpty())   seq.defaultSuffix = parser.value(suffixOpt);
        if (parser.isSet(noMetaOpt))       seq.defaultSaveMetadata = false;

        int result = runSequence(driver, seq);
        driver->disconnectCamera();
        return (result == 0) ? 0 : 1;
    }

    // --- Simple capture mode ---
    bool ok = false;
    int frameCount = parser.value(framesOpt).toInt(&ok);
    if (!ok || frameCount <= 0)
    {
        qCritical() << "Invalid --frames value:" << parser.value(framesOpt);
        driver->disconnectCamera();
        return 1;
    }

    QString outputDir = parser.value(outputOpt);
    QString format = parser.value(formatOpt).toLower();
    if (format != QStringLiteral("tiff") && format != QStringLiteral("csv"))
    {
        qWarning() << "Unknown format:" << format << "- using tiff";
        format = QStringLiteral("tiff");
    }
    QString prefix = parser.value(prefixOpt);
    QString suffix = parser.value(suffixOpt);
    bool saveMetadata = !parser.isSet(noMetaOpt);

    int captured = captureFrames(driver, frameCount, outputDir, format, prefix, suffix, saveMetadata);

    driver->disconnectCamera();
    return (captured >= 0) ? 0 : 1;
}
