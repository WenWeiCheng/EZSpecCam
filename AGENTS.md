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
│   ├── cli/            # CLI app (QCoreApplication)
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
| CLI entry point | `src/cli/main.cpp` | QCoreApplication, CommandLineParser |
| Build config | `CMakeLists.txt` / `CMakePresets.json` | msvc-debug, msvc-release, gcc-debug presets |
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
| `CommandLineParser` | Class | `src/cli/CommandLineParser.h` | CLI argument parsing |
| `CaptureController` | Class | `src/cli/CaptureController.h` | CLI capture workflow |

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

## NOTES
- `qcustomplot.cpp` is 32k lines — largest file; read-only
- Exiv2 for image metadata (optional, warns if missing)
- Camera drivers loaded via Qt plugin system (`QPluginLoader`); each has a `.json` descriptor
- Test executables are separate targets built with Qt Test framework (`Qt6::Test`)
- IDE: `.clangd` config present for LSP; `compile_commands.json` generated at build

## BUILD CONVENTIONS
- Root CMakeLists.txt: Uses `file(MAKE_DIRECTORY ...)` after setting output dirs — redundant butharmless
- Use `build_preset.bat` to configure and build.
