#include <QApplication>
#include <QSurfaceFormat>

#include "widgets/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setApplicationName("EZSpecCam");
    QCoreApplication::setApplicationVersion("1.0.0");
    QCoreApplication::setOrganizationName("EZSpecCam");

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