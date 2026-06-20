cmake_minimum_required(VERSION 3.20)
project(test_row_range_dialog)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets Test REQUIRED)

add_executable(test_row_range_dialog
    test_row_range_dialog.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/workers/FileLoaderWorker.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/dialogs/RowRangeDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/PostProcess.cpp
)

target_include_directories(test_row_range_dialog PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/app
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/gui/workers
    ${CMAKE_SOURCE_DIR}/src/app/formats
)

target_link_libraries(test_row_range_dialog PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)

add_test(NAME test_row_range_dialog COMMAND test_row_range_dialog)
