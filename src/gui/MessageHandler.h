#pragma once

namespace gui
{

/// Install qDebug/qInfo → stdout, qCritical/qFatal → stderr.
void installMessageHandler();

}