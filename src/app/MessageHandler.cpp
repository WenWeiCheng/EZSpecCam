#include "MessageHandler.h"

#include <QString>
#include <QTextStream>
#include <QtGlobal>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <cstdio>
#endif

namespace
{

void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    QTextStream out(type == QtCriticalMsg || type == QtFatalMsg ? stderr : stdout);
    out << msg << "\n";
    out.flush();
}

}

namespace app
{

void installMessageHandler()
{
    qInstallMessageHandler(messageHandler);
}

void attachParentConsoleIfAvailable()
{
#ifdef Q_OS_WIN
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        FILE *unused = nullptr;
        std::freopen("CONOUT$", "w", stdout);
        std::freopen("CONOUT$", "w", stderr);
        (void)unused;
    }
#endif
}

}
