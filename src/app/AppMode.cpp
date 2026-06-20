#include "AppMode.h"

#include <QString>
#include <QStringList>

namespace
{
const QStringList kHeadlessTriggers = {
    "--list",
    "--list-params",
    "--camera",
    "--set",
    "--frames",
    "--sequence",
    "--output",
    "--format",
    "--prefix",
    "--suffix",
    "--help",
    "--version"
};
}

namespace app
{

Mode parseAppMode(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (kHeadlessTriggers.contains(arg))
            return Mode::Headless;
    }
    return Mode::Windowed;
}

}
