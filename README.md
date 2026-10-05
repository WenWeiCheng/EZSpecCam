# EZSpecCam

[![License](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE)
[![Qt](https://img.shields.io/badge/Qt-6.2+-green.svg)](https://www.qt.io/)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)

一个基于 Qt 的科学光谱相机控制应用程序 —— 提供相机发现、连接、参数管理与图像采集功能。

## 功能特性

- **插件化相机驱动** —— 通过 Qt 插件系统在运行时加载相机驱动
- **参数管理** —— 配置 ROI、binning、曝光、增益等参数
- **图像与光谱显示** —— 实时帧显示并提供光谱可视化
- **Qt GUI 程序** —— 现代化的 Windows 界面
- **无界面 CLI** —— 支持脚本化采集、参数扫描与事件序列（详见 `src/cli/README.md`）

## 环境要求

- **Qt 6.2** 或更高版本（Windows 上使用 Qt 6.8）
- **C++17** 编译器（Windows 上为 MSVC 2022；Linux 上为 GCC 11+）
- **CMake 3.20+**
- `hk16011` 驱动另需：Linux、厂商 SDK（用 `HK16011_ROOT` 指定）。`libHK16011.so` 运行时依赖 `libusb-1.0` 与 `libserialport`（构建 SDK 本身时才需要对应的 `-dev` 包）

## 快速开始

### Linux

`qhyccd` 与 `hamamatsu` 仅支持 Windows，会被自动跳过。未安装 PICam SDK 时，`picam` 插件会带警告自行跳过；未设置 `HK16011_ROOT` 时，`hk16011` 插件同样带警告跳过。需要 Qt 6.2+、CMake 3.20+、GCC 11+ 以及 Ninja。

```bash
# 可选：启用 hk16011 驱动
export HK16011_ROOT=/path/to/hk16011/sdk/tree
```

```bash
git clone https://github.com/your-repo/EZSpecCam.git
cd EZSpecCam
./build_preset.sh debug      # 或者：release
QT_QPA_PLATFORM=offscreen ./build/linux-debug/bin/Debug/ezspeccam --list
QT_QPA_PLATFORM=offscreen ./build/linux-debug/bin/Debug/ezspeccam \
    --camera mock-001 --frames 1 --set exposure=10 --output /tmp/ezspec-out
```

测试脚本：`./run_tests.sh`（默认使用 `linux-debug`）。
打包脚本：`./deploy.sh`（需要 `$PATH` 中存在 `linuxdeployqt`）。

### Windows

需要 Qt 6.8+、MSVC 2022 与 CMake 3.20+。`qhyccd` 与 `hamamatsu` 依赖 `src/plugins/*/sdk/winlib/` 下的厂商 SDK 构建。

```powershell
git clone https://github.com/your-repo/EZSpecCam.git
cd EZSpecCam
.\build_preset.bat           # 或者：.\build_preset.bat debug / release
.\run_tests.bat
.\deploy.bat                 # 将 release 构建产物打包到 .\deploy\
.\build\msvc-debug\bin\Debug\ezspeccam.exe
```

## 架构

```
src/
├── core/           静态库 ezspeccam_core：驱动契约 + 数据类型 + 插件加载
│   ├── ICameraDriver.h            相机驱动接口（详见 ICameraDriver.md）
│   ├── CameraTypes.h              ROI / binning / 参数 / 错误 / 枚举
│   └── PluginLoader.{h,cpp}       Qt 插件发现 + ICameraDriver 实例化
├── formats/        帧写出器 + 附属元数据（CLI 与 GUI 共用）
│   ├── IImageFormatHandler.h      格式处理器接口
│   ├── FrameWriter.{h,cpp}        分发器（按扩展名选择处理器）
│   ├── TiffFormatHandler.{h,cpp}  TIFF 图像写出器
│   ├── CsvFormatHandler.{h,cpp}   CSV 行导出写出器
│   └── SaveTypes.h                持久化元数据结构
├── cli/            CLI 程序（QCoreApplication）→ 产出 ezspeccam
│   ├── main.cpp                   入口 —— QCommandLineParser 前端
│   ├── MessageHandler.{h,cpp}     qDebug/qWarning 路由
│   ├── HeadlessController.{h,cpp} 连接 → 采集 → 保存 → 断开 流程
│   ├── WaitStabilizer.{h,cpp}     温度 / 参数稳定化辅助
│   ├── ParameterClamper.{h,cpp}   参数范围钳制
│   ├── SequenceRunner.{h,cpp}     JSON 事件序列驱动（详见 src/cli/README.md）
│   ├── CliFormat.h                CLI 运行时的格式 / 控制台输出辅助
│   └── README.md                  完整选项列表与序列 schema
├── gui/            Qt GUI 程序（QApplication）→ 产出 ezspeccam-gui
│   ├── main.cpp                   入口 —— 创建 QApplication 与 MainWindow
│   ├── AppController.{h,cpp}      GUI 状态机，桥接驱动与控件
│   ├── MessageHandler.{h,cpp}     qDebug/qWarning 路由
│   ├── qcustomplot.{h,cpp}        内置的外部库（QCustomPlot 2.1.1），只读
│   ├── ui/                        各窗口 / 标签页的 UI 搭建代码
│   ├── widgets/                   主窗口、显示控件、对话框、后处理
│   └── workers/                   FileLoaderWorker / FileSaverWorker（独立线程）
└── plugins/        相机驱动插件
    ├── mock/       模拟相机（用于测试）
    ├── qhyccd/     QHYCCD 相机驱动（仅 Windows）
    ├── hamamatsu/  滨松相机驱动（仅 Windows）
    └── picam/      Princeton Instruments 相机驱动
```

### 核心（`src/core/`）

静态库 `ezspeccam_core`，定义相机驱动契约（`ICameraDriver`）、核心数据类型（ROI、binning、参数、错误）以及 Qt 插件加载器（命名空间 `app::plugins`）。CLI 与 GUI 都链接它。

### 应用

CLI 与 GUI 是**两个独立的可执行文件**，各自拥有独立的入口与 `MessageHandler`：

- `ezspeccam.exe` — CLI（控制台子系统；始终构建；正确写出 stdout，避免 PowerShell/cmd 提示符重复回显）。
- `ezspeccam-gui.exe` — GUI（Release 下为 `WIN32` 子系统，Debug 下为控制台子系统，使 `qDebug` 能输出到 IDE/终端）。

两者共用的基础设施：

- **插件发现**（`src/core/PluginLoader.cpp`，命名空间 `app::plugins`）—— 扫描插件根目录，加载每个 `*.dll` / `*.so`，并转型为 `ICameraDriver`。
- **日志**（`src/cli/MessageHandler.cpp`、`src/gui/MessageHandler.cpp`）—— 各程序安装自己的 `qDebug` 处理器，使消息在各自模式下都能稳定输出到 stdout / stderr。
- **采集流程**（`src/cli/HeadlessController.cpp`、`src/cli/WaitStabilizer.cpp`）—— 供 CLI 使用：连接、等待温度稳定、采集 N 帧、逐帧保存，然后断开。
- **帧输出**（`src/formats/`）—— `FrameWriter` 以及各格式的 `TiffFormatHandler` / `CsvFormatHandler` 按扩展名选择处理器，并在每张图像旁写出 `_metadata.json` 附属文件。

### CLI（`src/cli/`）

无界面模式的完整实现。`main.cpp` 解析 `QCommandLineParser` 并组装 `HeadlessOptions`，交给 `HeadlessController` 执行；`--sequence <file.json>` 可执行脚本化任务。完整的选项列表与序列 schema 详见 `src/cli/README.md`。

### GUI（`src/gui/`）

Qt GUI 应用程序：相机发现、连接管理、参数配置、实时图像与光谱显示。`AppController` 持有状态机（`Disconnected → Connecting → Connected → Acquiring → Error`），并在 `ICameraDriver` 与各显示控件之间搭桥。

### 插件（`src/plugins/`）

每个相机驱动都是一个实现 `ICameraDriver` 的 Qt 插件。驱动在运行时加载 —— 新增相机无需重新编译。

## 支持的相机

| 驱动 | 类型 | 说明 | **已测试机型** | 驱动下载 |
|--------|------|-------|-------|-------|
| Mock | 模拟 | 用于开发与测试 | 无 | 无 |
| QHYCCD | 硬件 | 支持真实的 QHY 相机 | QHY268M | [下载](https://www.qhyccd.cn/download/) |
| Hamamatsu | 硬件 | 支持滨松相机 | C16091-10 | [下载](https://www.hamamatsu.com/jp/en/product/cameras/software/driver-software.html) |
| PI | 硬件 | 支持 Princeton 相机 | PIXIS100B,PIXIS400B | [下载](https://www.princetoninstruments.com.cn/products_driver.html) |
| HK16011 | 硬件 | CCD 光谱相机（Linux，仅厂商 SDK） | HK16011 | 随设备提供，见 `HK16011_ROOT` |

> **HK16011 说明**：温控环尚未接入真实传感器，`sensor_temp` / `environment_temp` /
> `tec_voltage` / `tec_current` 返回的是固件占位值，驱动会在参数描述中标注「未实现」。
> 详见 [`src/plugins/hk16011/AGENTS.md`](src/plugins/hk16011/AGENTS.md)。

## 构建

各平台的具体命令见[快速开始](#快速开始)。两个对应脚本分别是 `build_preset.bat`（Windows）与 `build_preset.sh`（Linux）。

### 构建选项

| 选项 | 默认值 | 说明 |
|--------|---------|-------------|
| `EZSPECCAM_BUILD_TESTS` | ON | 构建测试可执行文件 |
| `EZSPECCAM_BUILD_APP` | ON | 构建应用程序（产出 `ezspeccam` 与 `ezspeccam-gui` 两个可执行文件） |
| `EZSPECCAM_BUILD_PLUGINS` | ON | 构建相机驱动插件 |

缺失的可选 SDK 只会让对应插件带 `WARNING` 跳过，不会中断构建。需要把缺失升级为硬错误时，
传对应的 `EZSPECCAM_REQUIRE_<SDK>` 开关：

| 开关 | 对应插件 |
|--------|---------|
| `EZSPECCAM_REQUIRE_PICAM` | `picam` |
| `EZSPECCAM_REQUIRE_HK16011` | `hk16011` |

## 许可证

本项目基于 **GNU General Public License v3.0** 发行，完整条款见 [LICENSE](LICENSE)。

之所以选用 GPLv3 而不是更宽松的许可证，是因为项目内置了 [QCustomPlot](https://www.qcustomplot.com/) 2.1.1
（`src/gui/qcustomplot.{h,cpp}`，上游原始副本，未作修改）。QCustomPlot 以 GPLv3 或商业授权双重许可，
其源文件被直接编译进 `ezspeccam-gui` 可执行文件，因此 GPLv3 的 copyleft 条款会覆盖整个 GUI 程序。
QCustomPlot 同时提供商业授权；若需要在保持宽松许可证的前提下闭源分发 GUI，可向其作者咨询。

历史提交中，曾有以 BSD 3-Clause 发布的早期版本；自本次变更起改为 GPLv3。

本程序按「原样」提供，不附带任何明示或默示担保。QCustomPlot 版权归 Emanuel Eichhammer 所有，
其原始版权声明保留在源文件头部。
