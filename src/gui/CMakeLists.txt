cmake_minimum_required(VERSION 3.20)

# GUI binary.
# Debug build → console subsystem so qDebug output appears in the IDE/terminal.
# Release build → WIN32 subsystem so no console window flashes on launch.

project(ezspeccam_gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.2 COMPONENTS Core Gui Widgets PrintSupport OpenGL REQUIRED)
if(Qt6_VERSION VERSION_LESS 6.2)
    message(FATAL_ERROR "Qt version ${Qt6_VERSION} is too old. Requires Qt 6.2 or higher.")
endif()

find_package(OpenGL REQUIRED)
find_package(OpenMP REQUIRED)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    qt_add_executable(ezspeccam-gui)
else()
    qt_add_executable(ezspeccam-gui WIN32)
endif()

target_sources(ezspeccam-gui PRIVATE
    main.cpp
    MessageHandler.h
    MessageHandler.cpp
    DebugMacros.h
    AppController.h
    AppController.cpp
    qcustomplot.h
    qcustomplot.cpp
    widgets/MainWindow.h
    widgets/MainWindow.cpp
    widgets/PostProcess.h
    widgets/PostProcess.cpp
    ui/MainWindowUi.h
    ui/MainWindowUi.cpp
    widgets/config/CameraTab.h
    widgets/config/CameraTab.cpp
    ui/CameraTabUi.h
    ui/CameraTabUi.cpp
    widgets/config/LoadingIndicator.h
    widgets/config/LoadingIndicator.cpp
    widgets/config/ParameterWidgetFactory.h
    widgets/config/ParameterWidgetFactory.cpp
    widgets/config/DataTab.h
    widgets/config/DataTab.cpp
    ui/DataTabUi.h
    ui/DataTabUi.cpp
    widgets/config/PluginTab.h
    widgets/config/PluginTab.cpp
    widgets/config/CameraConfigDialog.h
    widgets/config/CameraConfigDialog.cpp
    ui/CameraConfigDialogUi.h
    ui/CameraConfigDialogUi.cpp
    widgets/display/StatisticsDialog.h
    widgets/display/StatisticsDialog.cpp
    widgets/display/ProfileWindow.h
    widgets/display/ProfileWindow.cpp
    widgets/display/ImageViewWidget.h
    widgets/display/ImageViewWidget.cpp
    widgets/display/SpectrumViewWidget.h
    widgets/display/SpectrumViewWidget.cpp
    workers/FileLoaderWorker.cpp
    ${CMAKE_SOURCE_DIR}/src/formats/IImageFormatHandler.h
    ${CMAKE_SOURCE_DIR}/src/formats/SaveTypes.h
    ${CMAKE_SOURCE_DIR}/src/formats/TiffFormatHandler.h
    ${CMAKE_SOURCE_DIR}/src/formats/TiffFormatHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/formats/CsvFormatHandler.h
    ${CMAKE_SOURCE_DIR}/src/formats/CsvFormatHandler.cpp
    widgets/dialogs/RowRangeDialog.h
    widgets/dialogs/RowRangeDialog.cpp
    widgets/dialogs/CalibrationDialog.h
    widgets/dialogs/CalibrationDialog.cpp
    widgets/dialogs/AcquireDarkFrameDialog.h
    widgets/dialogs/AcquireDarkFrameDialog.cpp
    widgets/dialogs/ScaleControlDialog.h
    widgets/dialogs/ScaleControlDialog.cpp
    widgets/dialogs/DisplayStyleDialog.h
    widgets/dialogs/DisplayStyleDialog.cpp
    workers/FileSaverWorker.h
    workers/FileSaverWorker.cpp
    workers/FileLoaderWorker.h
    workers/FileLoaderWorker.cpp
)

target_include_directories(ezspeccam-gui PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(ezspeccam-gui PRIVATE QCUSTOMPLOT_USE_OPENGL)

target_link_libraries(ezspeccam-gui PRIVATE
    ezspeccam_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
    Qt6::OpenGL
    OpenMP::OpenMP_CXX
    OpenGL::GL
)