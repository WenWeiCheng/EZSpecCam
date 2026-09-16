#pragma once

class QApplication;

namespace gui
{

/// Configure app, show main window, and run the event loop. Returns exit code.
int run(QApplication &app);

}