#include <QApplication>
#include <QSurfaceFormat>

#include "widgets/MainWindow.h"

namespace gui
{

int run(QApplication &app)
{
    QCoreApplication::setApplicationName("EZSpecCam");
    QCoreApplication::setApplicationVersion("1.0.0");
    QCoreApplication::setOrganizationName("EZSpecCam");

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

}
