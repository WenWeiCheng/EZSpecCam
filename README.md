# EZSpecCam

[![License](https://img.shields.io/badge/license-BSD--3--Clause-blue.svg)](LICENSE)
[![Qt](https://img.shields.io/badge/Qt-6.8+-green.svg)](https://www.qt.io/)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)

A Qt-based camera control application for scientific spectroscopy — discovery, connection, parameter management, and image capture.

## Features

- **Plugin-based camera drivers** — Load camera drivers at runtime via Qt's plugin system
- **Parameter management** — Configure ROI, binning, exposure, gain, and more
- **Image & spectrum view** — Live frame display with spectrum visualization
- **Qt GUI application** — Modern Windows UI
- **Headless CLI** — Scripted capture, parameter sweeps, and event sequences (see `src/cli/README.md`)

## Requirements

- **Qt 6.8** or higher
- **C++17** compiler (MSVC 2022)
- **CMake 3.20+**

## Quick Start

```bash
# Clone the repository
git clone https://github.com/your-repo/EZSpecCam.git
cd EZSpecCam

# Configure and build
.\build_preset.bat
```

Run the application:
```bash
./build/msvc-debug/bin/Debug/ezspeccam.exe
```

> Ensure Visual C++ v14 Redistributable and camera drivers are installed.

## Architecture

```
src/
├── core/           Camera driver interface + data types (static library)
├── app/            Unified application (CLI + GUI in one binary)
│   ├── AppMode     Runtime mode dispatch (no args → GUI, CLI flags → headless)
│   ├── plugins     Plugin loader (stateless namespace)
│   ├── formats     TIFF/CSV frame writers + dispatcher
│   ├── HeadlessController  connect → capture → save → disconnect pipeline
│   └── formats/    Format handlers + metadata sidecar (with softwareSettings)
├── cli/            CLI mode: QCommandLineParser + cli::run
├── gui/            GUI mode: QApplication + MainWindow + AppController
└── plugins/        Camera driver plugins
    ├── mock/       Simulated camera (for testing)
    ├── qhyccd/     QHYCCD camera driver
    ├── hamamatsu/  Hamamatsu camera driver
    └── picam/      Princeton Instruments camera driver
```

### Core (`src/core/`)

Static library defining the camera driver contract (`ICameraDriver`) and core data types (ROIs, binning, parameters, errors).

### CLI (`src/cli/`)

Headless command-line mode of the unified `ezspeccam.exe` for scripted capture, single-shot parameter sweeps, and JSON event sequences (e.g. configure → wait-for-stable-temperature → capture). See `src/cli/README.md` for options and the sequence schema.

### GUI (`src/gui/`)

Qt GUI application with camera discovery, connection management, parameter configuration, and live image/spectrum display.

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

```powershell
.\build_preset.bat debug   # Debug build
.\build_preset.bat release # Release build
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `EZSPECCAM_BUILD_TESTS` | ON | Build test executables |
| `EZSPECCAM_BUILD_APP` | ON | Build the unified `ezspeccam` application (CLI + GUI) |
| `EZSPECCAM_BUILD_PLUGINS` | ON | Build camera driver plugins |

## License

This project is licensed under the BSD 3-Clause License — see [LICENSE](LICENSE) for details.
