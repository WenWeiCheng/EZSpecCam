@echo off
echo ============================================
echo EZSpecCam Build (MSVC Debug)
echo ============================================

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo ERROR: Failed to setup MSVC environment
    exit /b 1
)

echo MSVC environment ready

set SOURCE_DIR=D:\10_Projects\2502-Sw-EZSpecCam-shadow
set BUILD_DIR=%SOURCE_DIR%\build\msvc-debug
set QT_DIR=C:\Qt\6.8.2\msvc2022_64

if exist "%BUILD_DIR%" (
    echo Cleaning previous build...
    rmdir /s /q "%BUILD_DIR%"
)

echo Configuring CMake...
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S "%SOURCE_DIR%" -B "%BUILD_DIR%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_PREFIX_PATH="%QT_DIR%" ^
  -DEZSPECCAM_BUILD_GUI=OFF ^
  -DEZSPECCAM_BUILD_CLI=OFF ^
  -DEZSPECCAM_BUILD_PLUGINS=ON ^
  -DEZSPECCAM_BUILD_TESTS=ON

if errorlevel 1 (
    echo ERROR: CMake configure failed
    exit /b 1
)

echo.
echo Building...
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "%BUILD_DIR%" -- -j%NUMBER_OF_PROCESSORS%

if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo ============================================
echo BUILD SUCCESSFUL
echo ============================================
exit /b 0
