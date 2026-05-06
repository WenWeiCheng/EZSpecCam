@echo off
echo ============================================
echo EZSpecCam Test Runner
echo ============================================

set BUILD_DIR=D:\10_Projects\2502-Sw-EZSpecCam-shadow\build\msvc-debug
set QT_DIR=C:\Qt\6.8.2\msvc2022_64
set PATH=%QT_DIR%\bin;%PATH%

cd /d "%BUILD_DIR%" || exit /b 1

echo.
echo Running all tests...
echo ============================================

ctest --output-on-failure -C Debug
exit /b %ERRORLEVEL%
