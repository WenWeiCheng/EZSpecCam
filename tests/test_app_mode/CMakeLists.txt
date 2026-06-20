cmake_minimum_required(VERSION 3.20)
project(test_app_mode)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Test REQUIRED)

add_executable(test_app_mode test_app_mode.cpp
    ${CMAKE_SOURCE_DIR}/src/app/AppMode.h
    ${CMAKE_SOURCE_DIR}/src/app/AppMode.cpp
)

target_include_directories(test_app_mode PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_app_mode PRIVATE
    Qt6::Core Qt6::Test
)

add_test(NAME test_app_mode COMMAND test_app_mode)
