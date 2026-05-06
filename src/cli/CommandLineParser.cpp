#include "CommandLineParser.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>

CommandLineParser::CommandLineParser()
{
}

bool CommandLineParser::parse(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("ezspeccam");
    QCoreApplication::setApplicationVersion("1.0.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("EZSpecCam - Command-line spectral camera control");

    QCommandLineOption helpOption("help", "Display this help message");
    parser.addOption(helpOption);

    QCommandLineOption listCamerasOption(QStringList() << "l" << "list-cameras",
        "List all available cameras");
    parser.addOption(listCamerasOption);

    QCommandLineOption cameraIdOption(QStringList() << "c" << "camera",
        "Specify camera ID to use", "id");
    parser.addOption(cameraIdOption);

    QCommandLineOption captureOption(QStringList() << "C" << "capture",
        "Start capture mode");
    parser.addOption(captureOption);

    QCommandLineOption countOption(QStringList() << "n" << "count",
        "Number of frames to capture (0=continuous, default: 0)", "n");
    parser.addOption(countOption);

    QCommandLineOption outputOption(QStringList() << "o" << "output",
        "Output directory for captured frames", "dir");
    parser.addOption(outputOption);

    QCommandLineOption formatOption(QStringList() << "f" << "format",
        "Image format: tiff or jpg (default: tiff)", "format");
    parser.addOption(formatOption);

    QCommandLineOption exposureOption(QStringList() << "e" << "exposure",
        "Exposure time in milliseconds (default: 100)", "ms");
    parser.addOption(exposureOption);

    QCommandLineOption gainOption(QStringList() << "g" << "gain",
        "Gain value (default: 1.0)", "value");
    parser.addOption(gainOption);

    if (!parser.parse(QCoreApplication::arguments())) {
        m_errorMessage = parser.errorText();
        return false;
    }

    m_args.help = parser.isSet(helpOption);
    m_args.listCameras = parser.isSet(listCamerasOption);
    m_args.capture = parser.isSet(captureOption);

    if (parser.isSet(cameraIdOption)) {
        m_args.cameraId = parser.value(cameraIdOption);
    }

    if (parser.isSet(countOption)) {
        bool ok;
        int count = parser.value(countOption).toInt(&ok);
        if (!ok) {
            m_errorMessage = "Invalid count value: must be an integer";
            return false;
        }
        m_args.captureCount = count;
    }

    if (parser.isSet(outputOption)) {
        m_args.outputDir = parser.value(outputOption);
    }

    if (parser.isSet(formatOption)) {
        QString format = parser.value(formatOption).toLower();
        if (format != "tiff" && format != "jpg") {
            m_errorMessage = "Invalid format: must be 'tiff' or 'jpg'";
            return false;
        }
        m_args.format = format;
    }

    if (parser.isSet(exposureOption)) {
        bool ok;
        double exposure = parser.value(exposureOption).toDouble(&ok);
        if (!ok) {
            m_errorMessage = "Invalid exposure value: must be a number";
            return false;
        }
        if (exposure <= 0) {
            m_errorMessage = "Invalid exposure value: must be positive";
            return false;
        }
        m_args.exposure = exposure;
    }

    if (parser.isSet(gainOption)) {
        bool ok;
        double gain = parser.value(gainOption).toDouble(&ok);
        if (!ok) {
            m_errorMessage = "Invalid gain value: must be a number";
            return false;
        }
        if (gain < 0) {
            m_errorMessage = "Invalid gain value: must be non-negative";
            return false;
        }
        m_args.gain = gain;
    }

    return true;
}

const CommandLineArgs &CommandLineParser::args() const
{
    return m_args;
}

QString CommandLineParser::errorMessage() const
{
    return m_errorMessage;
}

bool CommandLineArgs::isValid() const
{
    if (listCameras) {
        return true;
    }

    if (cameraId.isEmpty()) {
        return false;
    }

    if (capture) {
        if (captureCount < 0) {
            return false;
        }
    }

    return true;
}

QString CommandLineArgs::helpText()
{
    return QStringLiteral(
        "Usage: ezspeccam [options]\n"
        "\n"
        "Options:\n"
        "  -l, --list-cameras       List all available cameras\n"
        "  -c, --camera <id>        Specify camera ID to use\n"
        "  -C, --capture            Start capture mode\n"
        "  -n, --count <n>          Number of frames (0=continuous, default: 0)\n"
        "  -o, --output <dir>       Output directory for captured frames\n"
        "  -f, --format <tiff|jpg>  Image format (default: tiff)\n"
        "  -e, --exposure <ms>      Exposure time in milliseconds (default: 100)\n"
        "  -g, --gain <value>       Gain value (default: 1.0)\n"
        "  -h, --help               Display this help message\n"
        "\n"
        "Examples:\n"
        "  ezspeccam --list-cameras\n"
        "  ezspeccam --camera mock --capture --output ./data\n"
        "  ezspeccam --camera mock --capture --count 10 --output ./data\n"
        "  ezspeccam --camera mock --capture --exposure 50 --gain 2.0\n"
        "\n"
        "Exit codes:\n"
        "  0 = Success\n"
        "  1 = Connection error\n"
        "  2 = Capture error\n"
        "  3 = Configuration error\n"
    );
}