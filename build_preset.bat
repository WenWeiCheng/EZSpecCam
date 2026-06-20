@echo off
setlocal

echo ============================================
echo EZSpecCam Build using CMakePresets
echo ============================================

REM ===========================================================
REM 1. Detect Visual Studio via vswhere.exe (official, supports
REM    all SKUs: Community, Professional, Enterprise, BuildTools,
REM    plus non-default install paths).
REM ===========================================================
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found at "%VSWHERE%".
    echo        Install Visual Studio 2022 with the "Desktop development with C++" workload.
    exit /b 1
)

set "VS_INSTALL_DIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_INSTALL_DIR=%%i"
)

if not defined VS_INSTALL_DIR (
    echo ERROR: No Visual Studio 2022 installation with the C++ workload was found.
    echo        Install Visual Studio 2022 with the "Desktop development with C++" workload.
    exit /b 1
)

if not exist "%VS_INSTALL_DIR%\VC\Auxiliary\Build\vcvars64.bat" (
    echo ERROR: vcvars64.bat not found under "%VS_INSTALL_DIR%".
    echo        The detected VS install appears to be missing the C++ workload.
    exit /b 1
)

echo Using Visual Studio at: %VS_INSTALL_DIR%
call "%VS_INSTALL_DIR%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo ERROR: Failed to initialize MSVC environment from "%VS_INSTALL_DIR%".
    exit /b 1
)

REM Use the Ninja that ships with VS (avoids picking up a stale Strawberry one on PATH).
set "PATH=%VS_INSTALL_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

echo MSVC environment ready
echo.

REM ===========================================================
REM 2. Validate required environment variables.
REM ===========================================================
if not defined QT_DIR (
    echo ERROR: QT_DIR environment variable is not set.
    echo        Set it to the Qt MSVC kit root, e.g.:
    echo            setx QT_DIR "C:\Qt\6.8.2\msvc2022_64"
    echo        Or create a CMakeUserPresets.json that overrides CMAKE_PREFIX_PATH.
    exit /b 1
)
if not exist "%QT_DIR%\lib\cmake\Qt6\Qt6Config.cmake" (
    echo ERROR: Qt6Config.cmake not found under "%QT_DIR%\lib\cmake\Qt6\".
    echo        QT_DIR must point to a Qt MSVC kit root, e.g. C:\Qt\6.8.2\msvc2022_64.
    exit /b 1
)
echo Using Qt at: %QT_DIR%

if defined PicamRoot (
    if exist "%PicamRoot%\Includes\picam.h" (
        echo Using PICam SDK at: %PicamRoot%
    ) else (
        echo WARNING: PicamRoot is set to "%PicamRoot%" but Includes\picam.h was not found.
        echo          The picam plugin will be skipped.
    )
) else (
    echo PICam SDK not configured ^(PicamRoot unset^). The picam plugin will be skipped.
)
echo.

REM ===========================================================
REM 3. Resolve source dir, build type, preset.
REM ===========================================================
set "SOURCE_DIR=%~dp0"
set "SOURCE_DIR=%SOURCE_DIR:~0,-1%"
echo Source directory: %SOURCE_DIR%

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=debug

if /i "%BUILD_TYPE%"=="debug" (
    set PRESET=msvc-debug
) else if /i "%BUILD_TYPE%"=="release" (
    set PRESET=msvc-release
) else (
    echo ERROR: Invalid build type '%BUILD_TYPE%'. Use 'debug' or 'release'.
    exit /b 1
)

echo Available presets:
cmake --list-presets -S "%SOURCE_DIR%"
echo.

echo Configuring with %PRESET% preset...
cmake --preset %PRESET% -S "%SOURCE_DIR%"
if errorlevel 1 (
    echo ERROR: CMake configure failed
    exit /b 1
)

echo.
echo Building with %PRESET% preset (4 cores)...
cmake --build --preset %PRESET% -j 4
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo ============================================
echo BUILD SUCCESSFUL
echo ============================================
endlocal
exit /b 0
