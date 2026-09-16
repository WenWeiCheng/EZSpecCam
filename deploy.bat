@echo off
setlocal enabledelayedexpansion

echo ============================================
echo EZSpecCam Deployment Script
echo ============================================

set "SCRIPT_DIR=%~dp0"
set "SOURCE_DIR=%SCRIPT_DIR%"
set "BUILD_DIR=%SOURCE_DIR%build\msvc-release"
set "DEPLOY_DIR=%SOURCE_DIR%deploy"

:: Check build exists
if not exist "%BUILD_DIR%" (
    echo ERROR: Build directory not found: %BUILD_DIR%
    echo Please run build_preset.bat release first.
    exit /b 1
)

:: QHYCCD SDK path
set "QHYCCD_SDK=C:\Program Files\QHYCCD\AllInOne\sdk\x64"

:: Create deployment directory
echo Creating deployment directory...
rmdir /S /Q "%DEPLOY_DIR%" 2>nul
mkdir "%DEPLOY_DIR%"
mkdir "%DEPLOY_DIR%\plugins\drivers"

::: Copy main executables (CLI + GUI). Release build: bin\Release; Debug build: bin\Debug.
set "SUFFIX=Release"
if not exist "%BUILD_DIR%\bin\Release\ezspeccam.exe" set "SUFFIX=Debug"
set "CLI_EXE=%BUILD_DIR%\bin\%SUFFIX%\ezspeccam.exe"
set "GUI_EXE=%BUILD_DIR%\bin\%SUFFIX%\ezspeccam-gui.exe"
if not exist "%CLI_EXE%" (
    echo ERROR: ezspeccam.exe not found
    exit /b 1
)
if not exist "%GUI_EXE%" (
    echo ERROR: ezspeccam-gui.exe not found
    exit /b 1
)
echo Copying executables...
copy /Y "%CLI_EXE%" "%DEPLOY_DIR%\"
copy /Y "%GUI_EXE%" "%DEPLOY_DIR%\"

::: windeployqt - collect Qt runtime DLLs for both binaries
set "QT_DIR=C:\Qt\6.8.2\msvc2022_64"
if not exist "%QT_DIR%\bin\windeployqt.exe" (
    echo ERROR: windeployqt.exe not found at %QT_DIR%\bin\
    exit /b 1
)

echo Running windeployqt for CLI...
"%QT_DIR%\bin\windeployqt.exe" ^
    --no-translations ^
    --no-compiler-runtime ^
    --no-opengl-sw ^
    "%DEPLOY_DIR%\ezspeccam.exe"
if errorlevel 1 (
    echo ERROR: windeployqt failed for ezspeccam.exe
    exit /b 1
)

echo Running windeployqt for GUI...
"%QT_DIR%\bin\windeployqt.exe" ^
    --no-translations ^
    --no-compiler-runtime ^
    --no-opengl-sw ^
    "%DEPLOY_DIR%\ezspeccam-gui.exe"
if errorlevel 1 (
    echo ERROR: windeployqt failed for ezspeccam-gui.exe
    exit /b 1
)

::: Copy camera driver plugins (auto-discover *.dll from build's deployed location)
set "PLUGIN_SRC_DIR="
if exist "%BUILD_DIR%\bin\Release\plugins\drivers" (
    set "PLUGIN_SRC_DIR=%BUILD_DIR%\bin\Release\plugins\drivers"
) else (
    set "PLUGIN_SRC_DIR=%BUILD_DIR%\bin\Debug\plugins\drivers"
)
echo Copying camera driver plugins from %PLUGIN_SRC_DIR%...
set "PLUGIN_COUNT=0"
for %%f in ("%PLUGIN_SRC_DIR%\*.dll") do (
    echo   %%~nxf
    copy /Y "%%f" "%DEPLOY_DIR%\plugins\drivers\" >nul
    set /a PLUGIN_COUNT+=1
)
if "!PLUGIN_COUNT!"=="0" (
    echo WARNING: No camera driver DLLs found in %PLUGIN_SRC_DIR%
)

:: Copy QHYCCD SDK DLLs
echo Copying QHYCCD SDK DLLs...
if not exist "%QHYCCD_SDK%\qhyccd.dll" (
    echo WARNING: qhyccd.dll not found at %QHYCCD_SDK%
) else (
    copy /Y "%QHYCCD_SDK%\qhyccd.dll" "%DEPLOY_DIR%\"
    copy /Y "%QHYCCD_SDK%\ftd2xx.dll" "%DEPLOY_DIR%\"
    copy /Y "%QHYCCD_SDK%\winusb.dll" "%DEPLOY_DIR%\"
    copy /Y "%QHYCCD_SDK%\tbb.dll" "%DEPLOY_DIR%\"
    :: MSVC runtime from QHYCCD SDK (may be needed)
    copy /Y "%QHYCCD_SDK%\msvcp90.dll" "%DEPLOY_DIR%\"
    copy /Y "%QHYCCD_SDK%\msvcr90.dll" "%DEPLOY_DIR%\"
)

echo.
echo ============================================
echo Deployment complete!
echo Output: %DEPLOY_DIR%
echo ============================================
echo.
echo Directory contents:
dir /B "%DEPLOY_DIR%"
echo.
echo plugins/drivers:
dir /B "%DEPLOY_DIR%\plugins\drivers" 2>nul || echo (empty)

exit /b 0
