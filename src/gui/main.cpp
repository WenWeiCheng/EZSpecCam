#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QSurfaceFormat>

#include "MessageHandler.h"
#include "Theme.h"
#include "widgets/MainWindow.h"

int main(int argc, char *argv[])
{
    gui::installMessageHandler();

    QApplication app(argc, argv);
    app.setApplicationName("EZSpecCam");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("EZSpecCam");

    QApplication::setStyle("Fusion");

    QCommandLineParser parser;
    parser.setApplicationDescription("EZSpecCam");
    parser.addHelpOption();
    parser.addVersionOption();
    // 默认跟随系统；给个开关是为了在多屏或远程桌面这类系统主题不可靠的场合能手动定死
    QCommandLineOption themeOption(QStringList("theme"),
                                   "Color scheme: <system|light|dark> (default: system).",
                                   "scheme",
                                   "system");
    parser.addOption(themeOption);
    parser.process(app);

    Theme::instance()->applyCommandLine(parser);
    Theme::instance()->initialize();

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);

    MainWindow window;
    window.show();
    return app.exec();
}
