# EZSpecCam

[![License](https://img.shields.io/badge/license-BSD--3--Clause-blue.svg)](LICENSE)
[![Qt](https://img.shields.io/badge/Qt-6.2+-green.svg)](https://www.qt.io/)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)

A Qt-based camera control application for scientific spectroscopy — discovery, connection, parameter management, and image capture.

## Features

- **Plugin-based camera drivers** — Load camera drivers at runtime via Qt's plugin system
- **Parameter management** — Configure ROI, binning, exposure, gain, and more
- **Image & spectrum view** — Live frame display with spectrum visualization
- **Qt GUI application** — Modern Windows UI
- **Headless CLI** — Scripted capture, parameter sweeps, and event sequences (see `src/cli/README.md`)

## Requirements

- **Qt 6.2** or higher (Qt 6.8 on Windows)
- **C++17** compiler (MSVC 2022 on Windows; GCC 11+ on Linux)
- **CMake 3.20+**

## Quick Start

### Linux

`qhyccd` and `hamamatsu` are Windows-only and are skipped automatically. `picam` self-skips with a warning when the PICam SDK is not installed. Requires Qt 6.2+, CMake 3.20+, GCC 11+, and Ninja.

```bash
git clone https://github.com/your-repo/EZSpecCam.git
cd EZSpecCam
./build_preset.sh debug      # or: release
QT_QPA_PLATFORM=offscreen ./build/linux-debug/bin/Debug/ezspeccam --list
QT_QPA_PLATFORM=offscreen ./build/linux-debug/bin/Debug/ezspeccam \
    --camera mock-001 --frames 1 --set exposure=10 --output /tmp/ezspec-out
```

Test runner: `./run_tests.sh` (default `linux-debug`).
Distribution bundle: `./deploy.sh` (requires `linuxdeployqt` on `$PATH`).

### Windows

Requires Qt 6.8+, MSVC 2022, and CMake 3.20+. `qhyccd` and `hamamatsu` build against vendor SDKs in `src/plugins/*/sdk/winlib/`.

```powershell
git clone https://github.com/your-repo/EZSpecCam.git
cd EZSpecCam
.\build_preset.bat           # or: .\build_preset.bat debug / release
.\run_tests.bat
.\deploy.bat                 # bundles release build into .\deploy\
.\build\msvc-debug\bin\Debug\ezspeccam.exe
```

## Architecture

```
src/
├── core/           Camera driver interface + data types (static library)
├── app/            Unified application (CLI + GUI in one binary)
│   ├── main.cpp           Entry point — selects mode and launches CLI or GUI
│   ├── AppMode            Runtime mode dispatch (no args → GUI, CLI flags → headless)
│   ├── MessageHandler     qDebug/qWarning routing (Win32 OutputDebugString / stderr)
│   ├── PluginLoader       Qt-plugin discovery + ICameraDriver instantiation
│   ├── HeadlessController connect → capture → save → disconnect pipeline
│   ├── WaitStabilizer     Temperature / parameter stabilization helper
│   ├── SequenceRunner     JSON event-sequence driver (see src/cli/README.md)
│   ├── CliFormat          Console output helpers for CLI runs
│   └── formats/           Frame writers + sidecar metadata
│       ├── FrameWriter          Dispatcher (select handler by extension)
│       ├── IImageFormatHandler  Format-handler interface
│       ├── TiffFormatHandler    TIFF image writer
│       ├── CsvFormatHandler     CSV row-export writer
│       └── SaveTypes            Persisted metadata schema
├── cli/            CLI mode glue: CliMain.cpp + QCommandLineParser front-end
├── gui/            GUI mode: QApplication + MainWindow + AppController
└── plugins/        Camera driver plugins
    ├── mock/       Simulated camera (for testing)
    ├── qhyccd/     QHYCCD camera driver (Windows-only)
    ├── hamamatsu/  Hamamatsu camera driver (Windows-only)
    └── picam/      Princeton Instruments camera driver
```

### Core (`src/core/`)

Static library defining the camera driver contract (`ICameraDriver`) and core data types (ROIs, binning, parameters, errors).

### App (`src/app/`)

Single executable that hosts both CLI and GUI. `main.cpp` dispatches to either based on argv; the rest of `app/` is shared infrastructure:

- **Mode dispatch** (`AppMode`) — pure function returning the requested mode.
- **Logging** (`MessageHandler`) — installs a `qDebug` handler so messages hit stderr / OutputDebugString / log files consistently across modes.
- **Plugin discovery** (`PluginLoader`) — scans `<binary>/plugins/drivers/`, loads each `*.so` / `*.dll`, and casts to `ICameraDriver`.
- **Capture pipeline** (`HeadlessController`, `WaitStabilizer`) — sequence used by CLI to connect, wait for stable temperature, capture N frames, save each, then disconnect.
- **Frame output** (`formats/FrameWriter` + per-format handlers) — picks the handler by extension and writes a sidecar `_metadata.json` next to every image.

### CLI (`src/cli/`)

Front-end parser for the headless mode. `CliMain.cpp` wires `QCommandLineParser` into the `HeadlessController` and supports `--sequence <file.json>` for scripted runs. See `src/cli/README.md` for the full option list and sequence schema.

### GUI (`src/gui/`)

Qt GUI application: camera discovery, connection management, parameter configuration, live image and spectrum display. `AppController` owns the state machine (`Disconnected → Connecting → Connected → Acquiring → Error`) and bridges `ICameraDriver` to the view widgets.

### Plugins (`src/plugins/`)

Each camera driver is a Qt plugin implementing `ICameraDriver`. Drivers are loaded at runtime — no recompilation needed to add new cameras.

## Supported Cameras

| Driver | Type | Notes | **Tested on** | Drivers |
|--------|------|-------|-------|-------|
| Mock | Simulated | For development and testing | None | None |
| QHYCCD | Hardware | Real QHY camera support | QHY268M | [download](https://www.qhyccd.cn/download/) |
| Hamamatsu | Hardware | Hamamatsu camera support | C16091-10 | [download](https://www.hamamatsu.com/jp/en/product/cameras/software/driver-software.html) |
| PI | Hardware | Princeton Camera support | PIXIS100B,PIXIS400B | [download](https://www.princetoninstruments.com.cn/products_driver.html) |

## Building

See [Quick Start](#quick-start) for the platform-specific commands. The two scripts/mirrors are `build_preset.bat` (Windows) and `build_preset.sh` (Linux).

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `EZSPECCAM_BUILD_TESTS` | ON | Build test executables |
| `EZSPECCAM_BUILD_APP` | ON | Build the unified `ezspeccam` application (CLI + GUI) |
| `EZSPECCAM_BUILD_PLUGINS` | ON | Build camera driver plugins |

## License

This project is licensed under the BSD 3-Clause License — see [LICENSE](LICENSE) for details.
