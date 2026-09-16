#include <QApplication>
#include <QSurfaceFormat>

#include "MessageHandler.h"
#include "widgets/MainWindow.h"

int main(int argc, char *argv[])
{
    gui::installMessageHandler();

    QApplication app(argc, argv);
    app.setApplicationName("EZSpecCam");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("EZSpecCam");

    QApplication::setStyle("Fusion");

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