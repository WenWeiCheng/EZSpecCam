#include <QApplication>
#include <QCoreApplication>

#include "AppMode.h"
#include "MessageHandler.h"

namespace cli { int run(int argc, char *argv[], QCoreApplication &app); }
namespace gui { int run(QApplication &app); }

int main(int argc, char *argv[])
{
    app::installMessageHandler();

    const app::Mode mode = app::parseAppMode(argc, argv);

    if (mode == app::Mode::Headless)
    {
        app::attachParentConsoleIfAvailable();
        QCoreApplication coreApp(argc, argv);
        return cli::run(argc, argv, coreApp);
    }

    QApplication guiApp(argc, argv);
    return gui::run(guiApp);
}
