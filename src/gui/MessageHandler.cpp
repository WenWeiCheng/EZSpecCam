#include "MessageHandler.h"

#include <QString>
#include <QTextStream>
#include <QtGlobal>

namespace
{

void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    QTextStream out(type == QtCriticalMsg || type == QtFatalMsg ? stderr : stdout);
    out << msg << "\n";
    out.flush();
}

}

namespace gui
{

void installMessageHandler()
{
    qInstallMessageHandler(messageHandler);
}

}