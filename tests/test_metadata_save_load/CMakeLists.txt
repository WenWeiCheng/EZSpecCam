cmake_minimum_required(VERSION 3.20)
project(test_metadata_save_load)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Gui Test REQUIRED)

add_executable(test_metadata_save_load
    test_metadata_save_load.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/workers/formats/TiffFormatHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/workers/formats/CsvFormatHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/workers/FileLoaderWorker.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/workers/IImageFormatHandler.h
    ${CMAKE_SOURCE_DIR}/src/gui/workers/SaveTypes.h
)

target_include_directories(test_metadata_save_load PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui/workers
    ${CMAKE_SOURCE_DIR}/src/gui/workers/formats
)

target_link_libraries(test_metadata_save_load PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Test
)

add_test(NAME test_metadata_save_load COMMAND test_metadata_save_load)
