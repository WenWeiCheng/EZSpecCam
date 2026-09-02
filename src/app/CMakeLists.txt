cmake_minimum_required(VERSION 3.20)

# Unified EZSpecCam executable (CLI + GUI in one binary).
# Mode is selected at runtime by argv; see src/app/main.cpp.

project(ezspeccam_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(EZSPECCAM_TARGET ezspeccam)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    qt_add_executable(${EZSPECCAM_TARGET})
else()
    qt_add_executable(${EZSPECCAM_TARGET} WIN32)
endif()

find_package(Qt6 6.2 COMPONENTS Core Gui Widgets PrintSupport OpenGL REQUIRED)
if(Qt6_VERSION VERSION_LESS 6.2)
    message(FATAL_ERROR "Qt version ${Qt6_VERSION} is too old. Requires Qt 6.2 or higher.")
endif()

find_package(OpenGL REQUIRED)
find_package(OpenMP REQUIRED)

target_link_libraries(${EZSPECCAM_TARGET} PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::PrintSupport Qt6::OpenGL
    OpenMP::OpenMP_CXX
    OpenGL::GL
)

target_compile_definitions(${EZSPECCAM_TARGET} PRIVATE QCUSTOMPLOT_USE_OPENGL)

target_include_directories(${EZSPECCAM_TARGET} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/app
    ${CMAKE_SOURCE_DIR}/src/cli
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/gui/workers
    ${CMAKE_SOURCE_DIR}/src/app/formats
)

target_sources(${EZSPECCAM_TARGET} PRIVATE
    main.cpp
    AppMode.h AppMode.cpp
    MessageHandler.h MessageHandler.cpp
    PluginLoader.h PluginLoader.cpp
    HeadlessController.h HeadlessController.cpp
    WaitStabilizer.h WaitStabilizer.cpp
    SequenceRunner.h SequenceRunner.cpp
    formats/FrameWriter.h formats/FrameWriter.cpp
    formats/IImageFormatHandler.h formats/SaveTypes.h
    formats/TiffFormatHandler.h formats/TiffFormatHandler.cpp
    formats/CsvFormatHandler.h formats/CsvFormatHandler.cpp

    ${CMAKE_SOURCE_DIR}/src/cli/CliMain.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/GuiMain.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/DebugMacros.h
    ${CMAKE_SOURCE_DIR}/src/gui/AppController.h ${CMAKE_SOURCE_DIR}/src/gui/AppController.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/qcustomplot.h ${CMAKE_SOURCE_DIR}/src/gui/qcustomplot.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/MainWindow.h ${CMAKE_SOURCE_DIR}/src/gui/widgets/MainWindow.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/PostProcess.h ${CMAKE_SOURCE_DIR}/src/gui/widgets/PostProcess.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/ui/MainWindowUi.h ${CMAKE_SOURCE_DIR}/src/gui/ui/MainWindowUi.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/config/CameraTab.h ${CMAKE_SOURCE_DIR}/src/gui/widgets/config/CameraTab.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/ui/CameraTabUi.h ${CMAKE_SOURCE_DIR}/src/gui/ui/CameraTabUi.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/config/LoadingIndicator.h ${CMAKE_SOURCE_DIR}/src/gui/widgets/config/LoadingIndicator.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/config/ParameterWidgetFactory.h ${CMAKE_SOURCE_DIR}/src/gui/widgets/config/ParameterWidgetFactory.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/config/DataTab.h ${CMAKE_SOURCE_DIR}/src/gui/widgets/config/DataTab.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/ui/DataTabUi.h ${CMAKE_SOURCE_DIR}/src/gui/ui/DataTabUi.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/config/PluginTab.h ${CMAKE_SOURCE_DIR}/src/gui/widgets/config/PluginTab.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/config/CameraConfigDialog.h ${CMAKE_SOURCE_DIR}/src/gui/widgets/config/CameraConfigDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/ui/CameraConfigDialogUi.h ${CMAKE_SOURCE_DIR}/src/gui/ui/CameraConfigDialogUi.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/ImageViewWidget.h ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/ImageViewWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/SpectrumViewWidget.h ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/SpectrumViewWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/StatisticsDialog.h ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/StatisticsDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/ProfileWindow.h ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/ProfileWindow.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/dialogs/RowRangeDialog.h ${CMAKE_SOURCE_DIR}/src/gui/widgets/dialogs/RowRangeDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/dialogs/ScaleControlDialog.h ${CMAKE_SOURCE_DIR}/src/gui/widgets/dialogs/ScaleControlDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/dialogs/DisplayStyleDialog.h ${CMAKE_SOURCE_DIR}/src/gui/widgets/dialogs/DisplayStyleDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/workers/FileSaverWorker.h ${CMAKE_SOURCE_DIR}/src/gui/workers/FileSaverWorker.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/workers/FileLoaderWorker.h ${CMAKE_SOURCE_DIR}/src/gui/workers/FileLoaderWorker.cpp
)
