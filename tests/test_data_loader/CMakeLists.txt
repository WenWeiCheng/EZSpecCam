cmake_minimum_required(VERSION 3.20)
project(test_data_loader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Test REQUIRED)

add_executable(test_data_loader test_data_loader.cpp)
target_link_libraries(test_data_loader PRIVATE Qt6::Core Qt6::Test EZSpecCam::Core)

add_test(NAME test_data_loader COMMAND test_data_loader)
