# EZSpecCam 知识库

**生成时间：** 2026-05-17
**提交：** 0418e4e
**分支：** main

## 概述
EZSpecCam 是一个基于 Qt 6.8、C++17 的相机控制应用程序，负责相机发现、连接、参数管理与图像采集。同时提供 GUI（QApplication）与 CLI（QCoreApplication）两个入口，驱动层采用插件化架构。

## 目录结构
```
├── src/
│   ├── core/           # 相机驱动接口 + 类型定义 → 详见 src/core/ICameraDriver.md
│   ├── formats/        # 图像格式处理器（TIFF、CSV、SaveTypes、FrameWriter）→ CLI 与 GUI 共用
│   ├── cli/            # CLI 程序（QCoreApplication）→ 详见 src/cli/README.md
│   ├── gui/            # Qt GUI 程序 → 详见 src/gui/AGENTS.md
│   └── plugins/        # 相机驱动插件 → 详见 src/plugins/AGENTS.md
├── tests/              # Qt Test 测试套件 → 详见 tests/AGENTS.md
└── build/              # CMake 构建产物（已加入 gitignore）
```

**会产出两个独立的可执行文件：**

- `ezspeccam.exe` — CLI（始终为控制台子系统；详见 src/cli/CMakeLists.txt）
- `ezspeccam-gui.exe` — GUI（Debug 为控制台子系统，使 qDebug 能输出到终端；Release 为 WIN32 子系统，避免弹出控制台窗口）

## 查阅指引

| 任务 | 位置 | 说明 |
|------|----------|-------|
| 相机驱动接口 | `src/core/ICameraDriver.h` | 约 15 个纯虚方法 + 5 个信号；Q_DECLARE_INTERFACE → 详见 `src/core/ICameraDriver.md` |
| 核心数据类型 | `src/core/CameraTypes.h` | ROI、binning、参数、错误、枚举 → 详见 `src/core/CameraTypes.md` |
| 应用控制器（GUI） | `src/gui/AppController.h` | 由 CameraManager 与 PluginManager 合并而来 |
| 主窗口 | `src/gui/widgets/MainWindow.h` | 带工具栏、菜单、信号的 QMainWindow |
| CLI 入口 | `src/cli/main.cpp` | `cli::installMessageHandler()` → `cli::attachParentConsoleIfAvailable()` → `QCoreApplication` → 解析 `QCommandLineParser` → `cli::run()` |
| 构建配置 | `CMakeLists.txt` / `CMakePresets.json` | configure/build/test 预设：msvc-debug、msvc-release、msvc-debug-gui、linux-debug、linux-release |
| 插件元数据 | `src/plugins/*/plugin.json` | 每个驱动一份 JSON 描述文件 |

## 代码地图
| 符号 | 类型 | 位置 | 职责 |
|--------|------|----------|------|
| `ICameraDriver` | 接口 | `src/core/ICameraDriver.h` | 所有相机驱动的契约 → 详细文档见 `src/core/ICameraDriver.md` |
| `AppController` | 类 | `src/gui/AppController.h` | GUI 状态机（Disconnected→Connecting→Connected→Acquiring→Error） |
| `MainWindow` | 类 | `src/gui/widgets/MainWindow.h` | 主窗口，持有全部控件 |
| `MainWindowUi` | 类 | `src/gui/ui/MainWindowUi.h` | 拆分出来的 UI 搭建（工具栏、菜单、停靠窗口） |
| `CameraTab` | 控件 | `src/gui/widgets/config/CameraTab.h` | 相机参数配置标签页 |
| `ImageViewWidget` | 控件 | `src/gui/widgets/display/ImageViewWidget.h` | 基于 QCustomPlot 的图像渲染 |
| `SpectrumViewWidget` | 控件 | `src/gui/widgets/display/SpectrumViewWidget.h` | 光谱图控件 |
| `MockCameraDriver` | 插件 | `src/plugins/mock/MockCameraDriver.cpp` | 用于测试的模拟相机 |
| `QHYCCDDriver` | 插件 | `src/plugins/qhyccd/QHYCCDDriver.cpp` | QHYCCD 真实硬件驱动 |
| `Hk16011Driver` | 插件 | `src/plugins/hk16011/Hk16011Driver.cpp` | HK16011 CCD 光谱相机驱动（Linux）→ 详见 `src/plugins/hk16011/AGENTS.md` |
| `SequenceRunner` | 类 | `src/cli/SequenceRunner.h` / `src/cli/SequenceRunner.cpp` | 解析并执行 JSON 事件序列脚本（`--sequence`） |

## 编码约定
- **C++17**，禁用扩展（`CMAKE_CXX_EXTENSIONS OFF`）
- **最低 Qt 6.2**（两处 `find_package(Qt6 6.2 ...)`）；Windows 侧建议并实际使用 6.8。`Q_OBJECT`/`signals`/`slots` 被识别为语句宏
- **缩进**：4 空格；**行宽上限**：120；**换行符**：LF（`\n`）
- **大括号**：Allman 风格（左大括号另起一行）；**指针对齐**：右对齐（`int *ptr`）
- **包含分组**：Qt → 本项目 → 标准库；不做自动排序
- **CMake**：`AUTOMOC ON`、`AUTOUIC ON`、`AUTORCC ON`；`CMAKE_EXPORT_COMPILE_COMMANDS ON`
- **命名**：类用 PascalCase，方法用 camelCase，MainWindow 的成员使用 `m_` 前缀（未强制全局执行）

## Git 提交规范
- **提交信息一律使用中文撰写**，正文说明「改了什么」以及「为什么改」，不要只写「修复问题」这类空泛描述。
- 沿用仓库现有的 Conventional Commits 前缀，例如 `fix(dark-frame): 修正暗帧累加器初始化参数顺序`。
- 一次提交只做一件事：修复与其配套的测试放在同一次提交，格式化或重命名单独提交。
- 提交前确认构建与测试均已通过（`run_tests.bat` / `./run_tests.sh`），并在正文中写明验证结果。
- 不要提交与本次改动无关的文件；构建产物、IDE 配置等本地文件不应入库。
- 本地个人项目开发，不需要创建分支，除非用户要求
- 分支合并用变基，保持提交树线性、清晰

## 禁止事项
- **禁止**修改 `src/gui/qcustomplot.*` —— 属于外部库（内置的 QCustomPlot 2.1.1）
- **禁止**在 CMakeLists.txt 和构建脚本中写入同级项目目录的绝对路径。
- **禁止**把与具体机器相关的路径提交到 `CMakePresets.json`。环境变量请使用 `$penv{NAME}`，或让用户自行创建本地 `CMakeUserPresets.json` 进行覆盖。详见[构建可移植性](#构建可移植性)。
- **禁止**在 `.bat` 文件中硬编码 Visual Studio 的安装路径。请使用 `vswhere.exe`（随 VS 安装器一起分发）来探测当前安装。
- **禁止**默认让缺失的可选 SDK（PICam、第三方相机 SDK）导致构建失败。应在 configure 阶段探测到缺失后输出 `WARNING`，并让对应的插件/target 直接 `return()`，以保证项目其余部分仍可正常构建。如需硬失败，请额外提供显式的 `EZSPECCAM_REQUIRE_<SDK>` 开关供用户选择。

## 构建与测试

Windows 与 Linux 各有一套等价脚本，请使用与当前平台匹配的那一套。

### Windows（PowerShell）

```powershell
# 配置 + 构建（默认 MSVC Debug）
& ".\build_preset.bat" 2>&1

# 配置 + 构建（MSVC Debug）
& ".\build_preset.bat debug" 2>&1

# 配置 + 构建（MSVC Release）
& ".\build_preset.bat release" 2>&1

# 运行测试
& ".\run_tests.bat" 2>&1
```
> **切勿在没有调用 vcvars64.bat 的情况下直接用 cmake 构建，否则构建会失败**。最佳做法是使用随仓库提供的 build_preset.bat 脚本，或参照它的写法自行编写脚本。

在 Windows 上执行某些测试可执行文件时，终端里看不到输出，此时应把结果输出到 txt 文件再读取内容。例如：

```
& "build\msvc-debug\bin\Debug\test_picam_driver_PIXIS100B.exe" -v2 -o test_output.txt 2>&1; Get-Content test_output.txt -Tail 100
# 只跑单个用例
& "build\msvc-debug\bin\Debug\test_picam_driver_PIXIS100B.exe" test_enumerate -v2 -o test_output.txt 2>&1; Get-Content test_output.txt -Tail 100
```

### Linux（bash）

```bash
# 配置 + 构建（默认 linux-debug）
./build_preset.sh              # 等价于 ./build_preset.sh debug

# 配置 + 构建（linux-release）
./build_preset.sh release

# 运行测试（默认 linux-debug）
./run_tests.sh                 # 等价于 ./run_tests.sh linux-debug
./run_tests.sh linux-release
```

**注意两个脚本的入参约定不同**：`build_preset.sh` 接收简写 `debug` / `release`，`run_tests.sh` 接收全名 `linux-debug` / `linux-release`。

Linux 上测试输出直接进终端，**不需要** Windows 那套「输出到 txt 再读取」的绕行办法。若在无显示器的机器（CI、容器、SSH 无 X 转发）上跑测试，需加上 `QT_QPA_PLATFORM=offscreen`：

```bash
QT_QPA_PLATFORM=offscreen ./run_tests.sh
```

也可以直接用 CMake 预设，**必须从仓库根目录执行**（`ctest --preset` 只在根目录读取 `CMakePresets.json`，不能 `cd` 进 build 目录）：

```bash
ctest --preset linux-debug
ctest --preset linux-release
```

产物位于 `build/linux-<config>/`，其中可执行文件在 `bin/Debug/` 或 `bin/Release/`，驱动插件在 `lib/Debug/` 或 `lib/Release/`。

## 备注
- `qcustomplot.cpp` 约 3.2 万行 —— 全项目最大的文件，只读
- 相机驱动通过 Qt 插件系统加载（`QPluginLoader`）；每个驱动带一份 `.json` 描述文件
- 测试可执行文件是独立 target，基于 Qt Test 框架（`Qt6::Test`）构建
- IDE：仓库内含 `.clangd` 配置供 LSP 使用；构建时会生成 `compile_commands.json`

## 构建约定
- 根 CMakeLists.txt：设置输出目录后仍调用 `file(MAKE_DIRECTORY ...)` —— 略显冗余但无害
- Windows 用 `build_preset.bat`，Linux 用 `build_preset.sh`，两者都只是对 `cmake --preset <platform>-<config>` 的封装。
- Linux 预设默认只构建 mock 驱动；`qhyccd` / `hamamatsu` 依赖厂商 Windows SDK，在 Linux 上不可用。
- Linux 上若设置了 `HK16011_ROOT`，则会额外构建 `hk16011` 插件及其测试（`test_hk16011_driver_HK16011`）。该测试在**没有接硬件时全部 `QSKIP`**，因此可以安全地留在 ctest 里。

## 构建可移植性

构建系统必须在 Windows 与 Linux 上都能正常工作，且不修改任何被 Git 跟踪的文件。

### Windows：需要用户侧配置

| 环境变量 | 示例 | 用途 |
|---------|---------|---------|
| `QT_DIR` | `C:\Qt\6.8.2\msvc2022_64` | Qt MSVC kit 根目录；`build_preset.bat` 会校验 `lib\cmake\Qt6\Qt6Config.cmake` 是否存在 |
| `PicamRoot` | `C:\Program Files\Princeton Instruments\PICam\v5` | PICam 5.x SDK 根目录；若缺失则 picam 插件会带警告跳过 |

`vswhere.exe`（随 VS 安装器一起分发）用于定位当前生效的 VS 安装 —— `build_preset.bat` 中不硬编码任何 SKU 或路径。

### Linux：依赖系统包，无需环境变量

Linux 侧除厂商 SDK 外不读任何环境变量，Qt 与工具链全部来自系统包（`qt6-base-dev`、`cmake`、`ninja-build`、`g++`）。`build_preset.sh` 会优先选用 `/usr/bin/cmake`，以避开某些环境（如 Xilinx/Vitis）PATH 中更靠前、依赖 `libidn.so.11` 的旧版 cmake。

| 环境变量 | 示例 | 用途 |
|---------|---------|---------|
| `HK16011_ROOT` | `/opt/hk16011` | HK16011 C SDK 源码树，需含 `include/hk16011.h` 与 `lib/libHK16011.so`；未设置时插件带警告跳过。插件编译本身只依赖该头文件与 `.so`，但 `libHK16011.so` 运行时需要 `libusb-1.0` 与 `libserialport`（构建 SDK 源码时才需对应的 `-dev` 包） |

`linux-debug` / `linux-release` 预设中写死了 `"CMAKE_PREFIX_PATH": "/usr"`。这在多数发行版上成立，但如果 Qt 装在别处（`/opt`、自定义 prefix），请用 `CMakeUserPresets.json` 覆盖，**不要**直接改动被跟踪的 `CMakePresets.json`。

### 按机器覆盖：`CMakeUserPresets.json`

如果用户无法或不想全局设置环境变量，可以在仓库根目录放置一个 `CMakeUserPresets.json`（该文件已在 `.gitignore` 中），通过 `inherits` 继承某个被跟踪的预设并覆盖特定的 `cacheVariables`：

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

随后执行 `cmake --preset msvc-debug-local`。CMake 会在 configure 阶段把用户预设合并到被跟踪的预设之上。

### 新增一个可选 SDK 依赖

1. 在受影响插件的 `CMakeLists.txt` 中定义 `option(EZSPECCAM_BUILD_PLUGIN_<NAME> "..." ON)` 以及一个默认 `OFF` 的 `EZSPECCAM_REQUIRE_<SDK>` 开关。
2. 先通过环境变量定位 SDK，其次是 `-D<VAR>=<path>` 覆盖，最后才回退到 `find_package()` / 头文件探测。
3. 如果 SDK 缺失且 `EZSPECCAM_REQUIRE_<SDK>` 为 `OFF`，则输出 `message(WARNING ...)` 并 `return()` —— **不要**调用 `message(FATAL_ERROR ...)`。项目其余部分必须仍能构建。
4. 在本节以及 `README.md` 的「构建」章节中记录该环境变量。
