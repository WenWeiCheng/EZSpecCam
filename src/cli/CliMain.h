#pragma once

class QCoreApplication;

namespace cli
{

/// Parse argv and dispatch headless operations. Returns process exit code.
int run(int argc, char *argv[], QCoreApplication &app);

}