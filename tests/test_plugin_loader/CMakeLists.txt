cmake_minimum_required(VERSION 3.20)
project(test_plugin_loader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Gui Test REQUIRED)

add_executable(test_plugin_loader test_plugin_loader.cpp
    ${CMAKE_SOURCE_DIR}/src/app/PluginLoader.h
    ${CMAKE_SOURCE_DIR}/src/app/PluginLoader.cpp
)

target_include_directories(test_plugin_loader PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/plugins/mock
)

target_link_libraries(test_plugin_loader PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Test
)

add_dependencies(test_plugin_loader mock_camera_driver)
target_compile_definitions(test_plugin_loader PRIVATE
    MOCK_PLUGIN_PATH="$<TARGET_FILE:mock_camera_driver>"
)

add_test(NAME test_plugin_loader COMMAND test_plugin_loader)
