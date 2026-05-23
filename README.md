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

## Requirements

- **Qt 6.8** or higher
- **C++17** compiler (MSVC 2022)
- **CMake 3.20+**
- **Exiv2** (optional — for image metadata support)

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
./build/gui/EZSpecCam.exe
```

> Ensure Visual C++ v14 Redistributable and camera drivers are installed.

## Architecture

```
src/
├── core/           Camera driver interface + data types (static library)
├── gui/            Qt GUI application
└── plugins/        Camera driver plugins
    ├── mock/       Simulated camera (for testing)
    ├── qhyccd/     QHYCCD camera driver
    └── hamamatsu/  Hamamatsu camera driver
```

### Core (`src/core/`)

Static library defining the camera driver contract (`ICameraDriver`) and core data types (ROIs, binning, parameters, errors).

### GUI (`src/gui/`)

Qt GUI application with camera discovery, connection management, parameter configuration, and live image/spectrum display.

### Plugins (`src/plugins/`)

Each camera driver is a Qt plugin implementing `ICameraDriver`. Drivers are loaded at runtime — no recompilation needed to add new cameras.

## Supported Cameras

| Driver | Type | Notes | Drivers |
|--------|------|-------|-------|
| Mock | Simulated | For development and testing | None |
| QHYCCD | Hardware | Real QHY camera support | [download](https://www.qhyccd.cn/download/) |
| Hamamatsu | Hardware | Hamamatsu camera support | [download](https://www.hamamatsu.com/jp/en/product/cameras/software/driver-software.html) |

## Building

```powershell
.\build_preset.bat debug   # Debug build
.\build_preset.bat release # Release build
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `EZSPECCAM_BUILD_TESTS` | ON | Build test executables |
| `EZSPECCAM_BUILD_GUI` | ON | Build GUI application |
| `EZSPECCAM_BUILD_PLUGINS` | ON | Build camera driver plugins |

## License

This project is licensed under the BSD 3-Clause License — see [LICENSE](LICENSE) for details.
