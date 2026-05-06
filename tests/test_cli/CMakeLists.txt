cmake_minimum_required(VERSION 3.20)
project(test_cli)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Test REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src)

add_executable(test_cli
    test_cli.h
    test_cli.cpp
)

target_link_libraries(test_cli PRIVATE
    Qt6::Core
    Qt6::Test
    ezspeccam_core
)

add_test(NAME test_cli COMMAND test_cli)