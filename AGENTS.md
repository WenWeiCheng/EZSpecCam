# EZSpecCam Knowledge Base

**Generated:** 2026-05-12
**Commit:** b84df97
**Branch:** main

## OVERVIEW
EZSpecCam is a Qt 6.8 C++17 application for camera control — discovery, connection, parameter management, and image capture. Supports both GUI (QApplication) and CLI (QCoreApplication) entry points with a plugin-based driver architecture.

## STRUCTURE
```
./
├── src/
│   ├── core/           # Camera driver interface + types (static lib)
│   ├── gui/            # Qt GUI app → see src/gui/AGENTS.md
│   ├── cli/            # CLI app (QCoreApplication)
│   └── plugins/        # Camera driver plugins → see src/plugins/AGENTS.md
├── tests/              # Qt Test suite → see tests/AGENTS.md
├── tutorial/           # Empty — placeholder for docs
└── build/              # CMake build outputs (gitignored)
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Camera driver interface | `src/core/ICameraDriver.h` | ~10 pure virtual methods; Q_DECLARE_INTERFACE |
| Core data types | `src/core/CameraTypes.h` | ROIs, binning, params, errors, enums |
| App controller (GUI) | `src/gui/AppController.h` | Merged CameraManager + PluginManager |
| MainWindow | `src/gui/widgets/MainWindow.h` | QMainWindow with toolbar, menus, signals |
| CLI entry point | `src/cli/main.cpp` | QCoreApplication, CommandLineParser |
| Build config | `CMakeLists.txt` / `CMakePresets.json` | msvc-debug, msvc-release, gcc-debug presets |
| Plugin metadata | `src/plugins/*/plugin.json` | JSON descriptors per driver |

## CODE MAP
| Symbol | Type | Location | Role |
|--------|------|----------|------|
| `ICameraDriver` | Interface | `src/core/ICameraDriver.h` | Contract for all camera drivers |
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
- **DO NOT** block the main thread in camera drivers — use async signals
- QHYCCD plugin is currently disabled in CMake (hardware-dependent)

## COMMANDS
```bash
# Configure + build (MSVC Debug)
& ".\build_preset.bat" 2>&1

# Run tests
& ".\run_tests.bat" 2>&1

# Manual CMake configure
cmake --preset msvc-debug
cmake --build build/msvc-debug
ctest --test-dir build/msvc-debug -C Debug --output-on-failure
```

## NOTES
- `qcustomplot.cpp` is 32k lines — largest file; read-only
- Exiv2 for image metadata (optional, warns if missing)
- OpenGL required for QCustomPlot hardware acceleration
- Camera drivers loaded via Qt plugin system (`QPluginLoader`); each has a `.json` descriptor
- Test executables are separate targets built with Qt Test framework (`Qt6::Test`)
- Some tests (`test_cli`, `test_qhyccd_driver`) are commented out in CMake
- IDE: `.clangd` config present for LSP; `compile_commands.json` generated at build
- `AGENTS_bak.md` is the old knowledge base — keep for reference, this file supersedes it
