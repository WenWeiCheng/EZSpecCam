cmake_minimum_required(VERSION 3.20)
project(test_post_process)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Test REQUIRED)

add_executable(test_post_process test_post_process.cpp)
target_link_libraries(test_post_process PRIVATE Qt6::Core Qt6::Test EZSpecCam::Core)

add_test(NAME test_post_process COMMAND test_post_process)
