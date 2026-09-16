#include <QCoreApplication>

#include "MessageHandler.h"
#include "CliMain.h"

int main(int argc, char *argv[])
{
    cli::installMessageHandler();
    cli::attachParentConsoleIfAvailable();

    QCoreApplication coreApp(argc, argv);
    return cli::run(argc, argv, coreApp);
}