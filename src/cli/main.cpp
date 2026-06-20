#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>

#include "app/HeadlessController.h"
#include "app/MessageHandler.h"
#include "SequenceRunner.h"

static QVariant parseSetValue(const QString &valueStr)
{
    bool ok = false;
    double d = valueStr.toDouble(&ok);
    if (ok) {
        if (valueStr.contains('.')) return d;
        qint64 i = valueStr.toLongLong(&ok);
        return ok ? QVariant(i) : QVariant(d);
    }
    return valueStr;
}

int main(int argc, char *argv[])
{
    app::installMessageHandler();
    app::attachParentConsoleIfAvailable();

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("EZSpecCam");
    QCoreApplication::setApplicationVersion("1.0.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("EZSpecCam headless camera control");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption listOpt("list", "List available cameras");
    QCommandLineOption listParamsOpt("list-params", "List all camera parameters");
    QCommandLineOption cameraOpt("camera", "Camera ID to connect to", "id");
    QCommandLineOption setOpt("set", "Set parameter (name=value)", "param");
    QCommandLineOption framesOpt("frames", "Number of frames to capture", "n", "1");
    QCommandLineOption outputOpt("output", "Output directory", "dir", ".");
    QCommandLineOption formatOpt("format", "Output format (tiff|csv)", "fmt", "tiff");
    QCommandLineOption prefixOpt("prefix", "Filename prefix", "str");
    QCommandLineOption suffixOpt("suffix", "Filename suffix", "str");
    QCommandLineOption sequenceOpt("sequence", "Event sequence JSON file", "file");

    parser.addOption(listOpt);
    parser.addOption(listParamsOpt);
    parser.addOption(cameraOpt);
    parser.addOption(setOpt);
    parser.addOption(framesOpt);
    parser.addOption(outputOpt);
    parser.addOption(formatOpt);
    parser.addOption(prefixOpt);
    parser.addOption(suffixOpt);
    parser.addOption(sequenceOpt);

    parser.process(app);

    app::HeadlessOptions opts;
    opts.listCameras = parser.isSet(listOpt);
    opts.listParams = parser.isSet(listParamsOpt);
    opts.cameraId = parser.value(cameraOpt);
    opts.frames = parser.value(framesOpt).toInt();
    opts.outputDir = parser.value(outputOpt);
    opts.format = parser.value(formatOpt).toLower();
    opts.prefix = parser.value(prefixOpt);
    opts.suffix = parser.value(suffixOpt);

    for (const QString &sv : parser.values(setOpt))
    {
        int eq = sv.indexOf('=');
        if (eq < 0) { qWarning() << "Invalid --set:" << sv; continue; }
        opts.setParameters[sv.left(eq).trimmed()] = parseSetValue(sv.mid(eq + 1).trimmed());
    }

    if (parser.isSet(sequenceOpt))
    {
        SequenceRunner seq;
        if (!seq.loadFromFile(parser.value(sequenceOpt))) {
            qCritical().noquote() << "Failed to load sequence:" << seq.errorString();
            return 1;
        }
        opts.sequence = seq.steps();
        if (opts.outputDir == "." && !seq.defaultOutputDir.isEmpty()) opts.outputDir = seq.defaultOutputDir;
        if (opts.format == "tiff" && !seq.defaultFormat.isEmpty())      opts.format = seq.defaultFormat;
        if (opts.prefix.isEmpty() && !seq.defaultPrefix.isEmpty())      opts.prefix = seq.defaultPrefix;
        if (opts.suffix.isEmpty() && !seq.defaultSuffix.isEmpty())      opts.suffix = seq.defaultSuffix;
    }

    return app::run(opts);
}
