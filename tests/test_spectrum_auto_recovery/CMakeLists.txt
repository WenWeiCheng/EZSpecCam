cmake_minimum_required(VERSION 3.20)
project(test_spectrum_auto_recovery)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Gui Test REQUIRED)

add_executable(test_spectrum_auto_recovery
    test_spectrum_auto_recovery.cpp
    ${CMAKE_SOURCE_DIR}/src/app/formats/TiffFormatHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/app/formats/CsvFormatHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/workers/FileLoaderWorker.cpp
    ${CMAKE_SOURCE_DIR}/src/app/formats/IImageFormatHandler.h
    ${CMAKE_SOURCE_DIR}/src/app/formats/SaveTypes.h
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/PostProcess.cpp
)

target_include_directories(test_spectrum_auto_recovery PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/app
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/gui/widgets
    ${CMAKE_SOURCE_DIR}/src/gui/workers
    ${CMAKE_SOURCE_DIR}/src/app/formats
)

target_link_libraries(test_spectrum_auto_recovery PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Test
)

add_test(NAME test_spectrum_auto_recovery COMMAND test_spectrum_auto_recovery)
