cmake_minimum_required(VERSION 3.20)

# Headless CLI binary (console subsystem).
# Reads commands from argv, connects to a camera via a plugin driver,
# configures parameters, captures frames or runs JSON event sequences.

project(ezspeccam_cli LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.2 COMPONENTS Core Gui REQUIRED)

# Console subsystem: stdout is always a valid handle, so qDebug / qInfo reach
# the parent terminal without any AttachConsole trickery. This fixes the prompt
# double-echo issue observed with the previous WIN32-subsystem unified binary.
qt_add_executable(ezspeccam)

target_sources(ezspeccam PRIVATE
    main.cpp
    MessageHandler.h
    MessageHandler.cpp
    HeadlessController.cpp
    WaitStabilizer.h
    WaitStabilizer.cpp
    SequenceRunner.h
    SequenceRunner.cpp
    CliFormat.h
    ${CMAKE_SOURCE_DIR}/src/formats/FrameWriter.h
    ${CMAKE_SOURCE_DIR}/src/formats/FrameWriter.cpp
    ${CMAKE_SOURCE_DIR}/src/formats/IImageFormatHandler.h
    ${CMAKE_SOURCE_DIR}/src/formats/SaveTypes.h
    ${CMAKE_SOURCE_DIR}/src/formats/TiffFormatHandler.h
    ${CMAKE_SOURCE_DIR}/src/formats/TiffFormatHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/formats/CsvFormatHandler.h
    ${CMAKE_SOURCE_DIR}/src/formats/CsvFormatHandler.cpp
)

target_include_directories(ezspeccam PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ezspeccam PRIVATE
    ezspeccam_core
    Qt6::Core
    Qt6::Gui
)