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

:: Copy main executable (exe is in bin\Release subdirectory for release, bin\Debug for debug)
echo Copying executable...
if exist "%BUILD_DIR%\bin\Release\ezspeccam_gui.exe" (
    set "GUI_EXE=%BUILD_DIR%\bin\Release\ezspeccam_gui.exe"
) else (
    set "GUI_EXE=%BUILD_DIR%\bin\Debug\ezspeccam_gui.exe"
)
if not exist "%GUI_EXE%" (
    echo ERROR: ezspeccam_gui.exe not found
    exit /b 1
)
copy /Y "%GUI_EXE%" "%DEPLOY_DIR%\"

:: windeployqt - collect Qt runtime DLLs
echo Running windeployqt...
set "QT_DIR=C:\Qt\6.8.2\msvc2022_64"
if not exist "%QT_DIR%\bin\windeployqt.exe" (
    echo ERROR: windeployqt.exe not found at %QT_DIR%\bin\
    exit /b 1
)

"%QT_DIR%\bin\windeployqt.exe" ^
    --no-translations ^
    --no-compiler-runtime ^
    --no-opengl-sw ^
    "%DEPLOY_DIR%\ezspeccam_gui.exe"

if errorlevel 1 (
    echo ERROR: windeployqt failed
    exit /b 1
)

:: Copy plugins (camera drivers from lib\Release or lib\Debug)
echo Copying camera driver plugins...
if exist "%BUILD_DIR%\lib\Release\qhyccd_camera_driver.dll" (
    copy /Y "%BUILD_DIR%\lib\Release\qhyccd_camera_driver.dll" "%DEPLOY_DIR%\plugins\drivers\"
    copy /Y "%BUILD_DIR%\lib\Release\mock_camera_driver.dll" "%DEPLOY_DIR%\plugins\drivers\"
) else (
    if exist "%BUILD_DIR%\lib\Debug\qhyccd_camera_driver.dll" (
        copy /Y "%BUILD_DIR%\lib\Debug\qhyccd_camera_driver.dll" "%DEPLOY_DIR%\plugins\drivers\"
        copy /Y "%BUILD_DIR%\lib\Debug\mock_camera_driver.dll" "%DEPLOY_DIR%\plugins\drivers\"
    )
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

:: Copy Exiv2 DLL if exists
echo Copying Exiv2...
set "EXIV2_DIR=C:\Users\weichen\vcpkg\installed\x64-windows\bin"
if exist "%EXIV2_DIR%\exiv2.dll" (
    copy /Y "%EXIV2_DIR%\exiv2.dll" "%DEPLOY_DIR%\"
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
