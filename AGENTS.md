# EZSpecCam Knowledge Base

**Generated:** 2026-05-17
**Commit:** 0418e4e
**Branch:** main

## OVERVIEW
EZSpecCam is a Qt 6.8 C++17 application for camera control — discovery, connection, parameter management, and image capture. Supports both GUI (QApplication) and CLI (QCoreApplication) entry points with a plugin-based driver architecture.

## STRUCTURE
```
./
├── src/
│   ├── core/           # Camera driver interface + types → see src/core/ICameraDriver.md
│   ├── gui/            # Qt GUI app → see src/gui/AGENTS.md
│   ├── cli/            # CLI app (QCoreApplication) — see src/cli/README.md
│   └── plugins/        # Camera driver plugins → see src/plugins/AGENTS.md
├── tests/              # Qt Test suite → see tests/AGENTS.md
└── build/              # CMake build outputs (gitignored)
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Camera driver interface | `src/core/ICameraDriver.h` | ~15 pure virtual methods + 5 signals; Q_DECLARE_INTERFACE → see `src/core/ICameraDriver.md` |
| Core data types | `src/core/CameraTypes.h` | ROIs, binning, params, errors, enums → see `src/core/CameraTypes.md` |
| App controller (GUI) | `src/gui/AppController.h` | Merged CameraManager + PluginManager |
| MainWindow | `src/gui/widgets/MainWindow.h` | QMainWindow with toolbar, menus, signals |
| CLI entry point | `src/cli/main.cpp` | QCoreApplication + Qt's `QCommandLineParser` (in-place) + `SequenceRunner` for JSON event sequences |
| Build config | `CMakeLists.txt` / `CMakePresets.json` | msvc-debug, msvc-release, msvc-debug-gui presets |
| Plugin metadata | `src/plugins/*/plugin.json` | JSON descriptors per driver |

## CODE MAP
| Symbol | Type | Location | Role |
|--------|------|----------|------|
| `ICameraDriver` | Interface | `src/core/ICameraDriver.h` | Contract for all camera drivers → detailed docs at `src/core/ICameraDriver.md` |
| `AppController` | Class | `src/gui/AppController.h` | GUI state machine (Disconnected→Connecting→Connected→Acquiring→Error) |
| `MainWindow` | Class | `src/gui/widgets/MainWindow.h` | Main Qt window; owns all widgets |
| `MainWindowUi` | Class | `src/gui/ui/MainWindowUi.h` | Separated UI setup (toolbar, menus, docks) |
| `CameraTab` | Widget | `src/gui/widgets/config/CameraTab.h` | Camera parameter configuration tab |
| `ImageViewWidget` | Widget | `src/gui/widgets/display/ImageViewWidget.h` | Frame rendering with QCustomPlot |
| `SpectrumViewWidget` | Widget | `src/gui/widgets/display/SpectrumViewWidget.h` | Spectrum plot widget |
| `MockCameraDriver` | Plugin | `src/plugins/mock/MockCameraDriver.cpp` | Simulated camera for testing |
| `QHYCCDDriver` | Plugin | `src/plugins/qhyccd/QHYCCDDriver.cpp` | Real QHYCCD hardware driver |
| `SequenceRunner` | Class | `src/cli/SequenceRunner.h` / `SequenceRunner.cpp` | Parses and runs JSON event-sequence scripts (`--sequence`) |

## CONVENTIONS
- **C++17**, no extensions (`CMAKE_CXX_EXTENSIONS OFF`)
- **Qt 6.8** minimum; `Q_OBJECT`/`signals`/`slots` recognized as statement macros
- **Indent**: 4 spaces; **Line limit**: 120; **EOL**: LF (`\n`)
- **Braces**: Allman style (brace on next line); **Pointer alignment**: right (`int *ptr`)
- **Include groups**: Qt → project → std; no auto-sorting
- **CMake**: `AUTOMOC ON`, `AUTOUIC ON`, `AUTORCC ON`; `CMAKE_EXPORT_COMPILE_COMMANDS ON`
- **Naming**: PascalCase classes, camelCase methods, m_ prefix for members in MainWindow (not universally enforced)

## ANTI-PATTERNS (THIS PROJECT)
- **DO NOT** modify `src/gui/qcustomplot.*` — external library (bundled QCustomPlot 2.1.1)
- **DO NOT** add absolute paths to sibling project directories in CMakeLists.txt and build scripts.
- **DO NOT** commit machine-specific paths to `CMakePresets.json`. Use `$penv{NAME}` for env vars, or have users create a local `CMakeUserPresets.json` for overrides. See [Build Portability](#build-portability).
- **DO NOT** hardcode Visual Studio install paths in `.bat` files. Use `vswhere.exe` (ships with the VS Installer) to detect the active installation.
- **DO NOT** make missing optional SDKs (PICam, third-party camera SDKs) fatal by default. They should be detected at configure time, emit a `WARNING`, and the corresponding plugin/target should `return()` so the rest of the project still builds. Add an explicit `EZSPECCAM_REQUIRE_<SDK>` opt-in flag for users who want a hard failure.

## Build and Test

```bash
# Configure + build (MSVC Debug default) 
& ".\build_preset.bat" 2>&1

# Configure + build (MSVC Debug)
& ".\build_preset.bat debug" 2>&1

# Configure + build (MSVC Release)
& ".\build_preset.bat release" 2>&1

# Run tests
& ".\run_tests.bat" 2>&1
```
> **Never use cmake to build directly without vcvars64.bat called. Otherwise the build will fail**. The best way to build is to use the provided build_preset.bat script or write a custom script in the same style as the provided build_preset.bat script.

When executing certain test executable, you can't see the output in terminal, so you should output a txt file and read its content to see the test result. For example:

```
& "build\msvc-debug\bin\Debug\test_picam_driver_PIXIS100B.exe" -v2 -o test_output.txt 2>&1; Get-Content test_output.txt -Tail 100
# test single function
& "build\msvc-debug\bin\Debug\test_picam_driver_PIXIS100B.exe" test_enumerate -v2 -o test_output.txt 2>&1; Get-Content test_output.txt -Tail 100
```

## NOTES
- `qcustomplot.cpp` is 32k lines — largest file; read-only
- Camera drivers loaded via Qt plugin system (`QPluginLoader`); each has a `.json` descriptor
- Test executables are separate targets built with Qt Test framework (`Qt6::Test`)
- IDE: `.clangd` config present for LSP; `compile_commands.json` generated at build

## BUILD CONVENTIONS
- Root CMakeLists.txt: Uses `file(MAKE_DIRECTORY ...)` after setting output dirs — redundant butharmless
- Use `build_preset.bat` to configure and build.

## Build Portability

The build system must work on any Windows machine with a Qt MSVC kit and a recent Visual Studio 2022 install, without modification of tracked files.

### Required user-side setup

| Env var | Example | Purpose |
|---------|---------|---------|
| `QT_DIR` | `C:\Qt\6.8.2\msvc2022_64` | Qt MSVC kit root; `build_preset.bat` validates `lib\cmake\Qt6\Qt6Config.cmake` exists |
| `PicamRoot` | `C:\Program Files\Princeton Instruments\PICam\v5` | PICam 5.x SDK root; if missing, picam plugin is skipped with a warning |

`vswhere.exe` (shipped with the VS Installer) is used to locate the active VS install — no hardcoded SKU or path in `build_preset.bat`.

### Per-machine overrides: `CMakeUserPresets.json`

If users cannot or do not want to set environment variables globally, they can drop a `CMakeUserPresets.json` at the repo root (already in `.gitignore`) that `inherits` from a tracked preset and overrides specific `cacheVariables`:

```jsonc
{
  "version": 6,
  "configurePresets": [
    {
      "name": "msvc-debug-local",
      "inherits": "msvc-debug",
      "cacheVariables": {
        "CMAKE_PREFIX_PATH": "D:/my-qt/6.8.0/msvc2022_64"
      }
    }
  ]
}
```

Then run `cmake --preset msvc-debug-local`. CMake merges user presets on top of the tracked ones at configure time.

### Adding a new optional SDK dependency

1. Define `option(EZSPECCAM_BUILD_PLUGIN_<NAME> "..." ON)` and an `EZSPECCAM_REQUIRE_<SDK>` flag (default `OFF`) in the affected plugin's `CMakeLists.txt`.
2. Locate the SDK via env var first, then `-D<VAR>=<path>` override, then fall back to `find_package()` / header probe.
3. If the SDK is absent and `EZSPECCAM_REQUIRE_<SDK>` is `OFF`, `message(WARNING ...)` and `return()` — do NOT call `message(FATAL_ERROR ...)`. The rest of the project must still build.
4. Document the env var in this section and in `README.md`'s Building chapter.
