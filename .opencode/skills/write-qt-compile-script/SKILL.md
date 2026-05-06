---
name: write-qt-compile-script
description: 当你需要编译或部署 qt 项目相关代码时，使用此技能。
---

## 前提

已有 qt 项目的源文件和 .pro 项目文件

## 编译工具

只能使用 qmake 和 msvc 工具编译。相关目录是：

- Qt 的 msvc 编译工具套装: `C:\Qt\6.8.2\msvc2022_64`

- MSVC 环境变量设置脚本：`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat`

## 构建规则

构建放在项目根目录的 build 目录的一个子目录下，这个子目录用 Qt 版本 + MSVC 版本 + 程序位数 + 构建版本命名，比如 `build/Qt_6_8_2_MSVC2022_64bit-Debug`

- 创建两个脚本，`build_debug.bat` 构建 Debug，`build_release` 构建 Release 版本
- 你只能使用 `build_debug.bat` 版本构建，`build_release` 留给用户

```
build/
  Qt_6_8_2_MSVC2022_64bit-Debug/
src/
project.pro
build_debug.bat
build_release.bat
```

脚本中不能有 pause 等需要用户交互的命令，这样脚本运行完成后会自动退出而不会等待用户按键。

**必须**检查
1. 构建目录有没有 Qt 依赖，比如 `Qt6Cored.dll` (debug) 或 `Qt6Core.dll`。
2. 环境变量系统路径有没有 Qt 依赖路径，比如 `C:\Qt\6.8.2\msvc2022_64\bin`

如果有，不需要部署；如果没有**必须**使用 `windeployqt` 部署，同构建出的程序放在一起（参考编译部署脚本示例）。这样才可以直接运行目标程序。

检查命令是否成功，使用 `if not errorlevel 0`。

## 编译部署脚本示例

- build_debug.bat

```
@echo off

set QT_DIR=C:\Qt\6.8.2\msvc2022_64
set SRC_DIR=%cd%
set BUILD_DIR=%cd%\build\Qt_6_8_2_MSVC2022_64bit-Debug

if not exist "%QT_DIR%" (
    echo Qt directory not found: %QT_DIR%
    exit /b 1
)
if not exist "%SRC_DIR%" (
    echo Source directory not found: %SRC_DIR%
    exit /b 1
)
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

cd %BUILD_DIR%

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64

"%QT_DIR%\bin\qmake.exe" "%SRC_DIR%\project.pro" -spec win32-msvc "CONFIG+=debug" "CONFIG+=console"
if not errorlevel 0 (
    echo qmake failed
    exit /b 1
)

nmake Debug
if not errorlevel 0 (
    echo nmake failed
    exit /b 1
)

where Qt6Cored.dll >nul 2>&1
if %errorlevel% equ 0 (
    echo Qt6Cored.dll found in PATH, skipping windeployqt
    goto :deploy_done
) 
if exist "%BUILD_DIR%\debug\Qt6Cored.dll" (
    echo Qt dependencies already deployed locally, skipping windeployqt
    goto :deploy_done
)

"%QT_DIR%\bin\windeployqt.exe" "%BUILD_DIR%\debug\applicatioin.exe"

:deploy_done
echo Deployment check complete
```

- build_release.bat

```
@echo off

set QT_DIR=C:\Qt\6.8.2\msvc2022_64
set SRC_DIR=%cd%
set BUILD_DIR=%cd%\build\Qt_6_8_2_MSVC2022_64bit-Release

if not exist "%QT_DIR%" (
    echo Qt directory not found: %QT_DIR%
    exit /b 1
)
if not exist "%SRC_DIR%" (
    echo Source directory not found: %SRC_DIR%
    exit /b 1
)
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

cd %BUILD_DIR%

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64

"%QT_DIR%\bin\qmake.exe" "%SRC_DIR%\project.pro" -spec win32-msvc "CONFIG+=release" "CONFIG+=console"
if not errorlevel 0 (
    echo qmake failed
    exit /b 1
)

nmake Release
if not errorlevel 0 (
    echo nmake failed
    exit /b 1
)

where Qt6Cored.dll >nul 2>&1
if %errorlevel% equ 0 (
    echo Qt6Cored.dll found in PATH, skipping windeployqt
    goto :deploy_done
) 
if exist "%BUILD_DIR%\debug\Qt6Cored.dll" (
    echo Qt dependencies already deployed locally, skipping windeployqt
    goto :deploy_done
)

"%QT_DIR%\bin\windeployqt.exe" "%BUILD_DIR%\debug\applicatioin.exe"

:deploy_done
echo Deployment check complete
```

## 执行命令

- 不允许使用新的命令窗中执行命令，不允许出现 `Start-Process cmd` 这样的命令
- 不允许执行完脚本后暂停，等待用户按键。执行完成后自动退出。
- 推荐命令：

```
powershell -Command "& { Set-Location 'path/to/script-dir'; .\build_debug.bat }"
```

如果是在 WSL 环境中：

```
powershell.exe -Command "& { Set-Location 'path/to/script-dir'; .\build_debug.bat }"
```
