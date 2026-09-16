#include <QApplication>

#include "MessageHandler.h"
#include "GuiMain.h"

int main(int argc, char *argv[])
{
    gui::installMessageHandler();

    QApplication app(argc, argv);
    return gui::run(app);
}