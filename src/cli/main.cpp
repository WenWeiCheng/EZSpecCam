#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QPluginLoader>
#include <csignal>
#include <iostream>

#include "CommandLineParser.h"
#include "CaptureController.h"
#include "core/ICameraDriver.h"

static volatile sig_atomic_t g_signalFlag = 0;

static void signalHandler(int signal)
{
    Q_UNUSED(signal);
    g_signalFlag = 1;
}

int main(int argc, char *argv[])
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("ezspeccam");
    QCoreApplication::setApplicationVersion("1.0.0");

    CommandLineParser parser;
    if (!parser.parse(argc, argv)) {
        std::cerr << "Error: " << qPrintable(parser.errorMessage()) << std::endl;
        return 3;
    }

    const CommandLineArgs &args = parser.args();

    if (args.help) {
        std::cout << qPrintable(CommandLineArgs::helpText()) << std::endl;
        return 0;
    }

    if (args.listCameras) {
        QStringList cameras = ICameraDriver::enumerate();
        if (cameras.isEmpty()) {
            std::cout << "No cameras found." << std::endl;
            return 0;
        }

        std::cout << "Available cameras:" << std::endl;
        for (const QString &cameraId : cameras) {
            std::cout << "  - " << qPrintable(cameraId) << std::endl;
        }
        return 0;
    }

    if (!args.capture || args.cameraId.isEmpty()) {
        std::cerr << "Error: --camera <id> and --capture are required for capture mode." << std::endl;
        std::cerr << "Use --help for usage information." << std::endl;
        return 3;
    }

    QDir pluginsDir(app.applicationDirPath());
    if (!pluginsDir.cd("plugins")) {
        pluginsDir = QDir(app.applicationDirPath() + "/../lib/plugins");
    }

    ICameraDriver *driver = nullptr;

    const auto entryList = pluginsDir.entryList(QDir::Files);
    for (const QString &fileName : entryList) {
        QString filePath = pluginsDir.absoluteFilePath(fileName);
        QPluginLoader loader(filePath);
        QObject *plugin = loader.instance();
        if (plugin) {
            if (auto *drv = qobject_cast<ICameraDriver*>(plugin)) {
                QStringList cameras = drv->enumerate();
                if (cameras.contains(args.cameraId)) {
                    driver = drv;
                    break;
                }
            }
        }
    }

    if (!driver) {
        std::cerr << "Error: Could not find camera driver for: " << qPrintable(args.cameraId) << std::endl;
        return 1;
    }

    CaptureController controller(args, driver, &g_signalFlag);
    int result = controller.execute();

    if (result != 0) {
        std::cerr << "Error: " << qPrintable(controller.lastError()) << std::endl;
    }

    return result;
}