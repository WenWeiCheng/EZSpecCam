cmake_minimum_required(VERSION 3.20)
project(test_post_process_manager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Test REQUIRED)

add_executable(test_post_process_manager test_post_process_manager.cpp)
target_link_libraries(test_post_process_manager PRIVATE Qt6::Core Qt6::Test EZSpecCam::Core)

add_test(NAME test_post_process_manager COMMAND test_post_process_manager)
