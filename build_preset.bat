@echo off
echo ============================================
echo EZSpecCam Build using CMakePresets
echo ============================================

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo ERROR: Failed to setup MSVC environment
    exit /b 1
)

echo MSVC environment ready
echo.

set "SOURCE_DIR=%~dp0"
set "SOURCE_DIR=%SOURCE_DIR:~0,-1%"
echo Source directory: %SOURCE_DIR%

:: Get build type from argument, default to debug
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=debug

:: Map build type to preset
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
exit /b 0
