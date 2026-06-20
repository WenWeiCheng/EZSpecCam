@echo off
setlocal

echo ============================================
echo EZSpecCam Test Runner
echo ============================================

set "BUILD_DIR=%~dp0build\msvc-debug"

if not defined QT_DIR (
    echo ERROR: QT_DIR environment variable is not set.
    echo        Set it to the Qt MSVC kit root, e.g. C:\Qt\6.8.2\msvc2022_64.
    exit /b 1
)
if not exist "%QT_DIR%\bin\qmake.exe" (
    echo ERROR: qmake.exe not found under "%QT_DIR%\bin\".
    echo        QT_DIR must point to a Qt MSVC kit root.
    exit /b 1
)

set "PATH=%QT_DIR%\bin;%PATH%"

if not exist "%BUILD_DIR%" (
    echo ERROR: Build directory not found: %BUILD_DIR%
    echo        Run build_preset.bat debug first.
    exit /b 1
)

cd /d "%BUILD_DIR%" || exit /b 1

echo.
echo Running all tests...
echo ============================================

ctest --output-on-failure -C Debug
exit /b %ERRORLEVEL%
