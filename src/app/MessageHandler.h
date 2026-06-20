#pragma once

namespace app
{

/// Install qDebug/qInfo → stdout, qCritical/qFatal → stderr.
void installMessageHandler();

/// Windows: AttachConsole(ATTACH_PARENT_PROCESS) and re-open stdout/stderr to CONOUT$.
/// Non-Windows: no-op.
void attachParentConsoleIfAvailable();

}
