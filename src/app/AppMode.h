#pragma once

namespace app
{
    enum class Mode { Headless, Windowed };

    Mode parseAppMode(int argc, char *argv[]);
}
