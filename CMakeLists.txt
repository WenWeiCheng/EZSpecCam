# EZSpecCam CMake Project
# Qt 6.8.2 C++ project with MSVC2022 and Linux compatibility

cmake_minimum_required(VERSION 3.20)

project(EZSpecCam VERSION 1.0.0 LANGUAGES CXX)

# ===========================================
# C++ Standard
# ===========================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ===========================================
# Qt Configuration
# ===========================================
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ===========================================
# Build Options
# ===========================================
option(EZSPECCAM_BUILD_TESTS "Build test executables" ON)
option(EZSPECCAM_BUILD_APP "Build unified EZSpecCam application" ON)
option(EZSPECCAM_BUILD_PLUGINS "Build plugins" ON)
# PICAM_SDK_ROOT is defined and consumed by src/plugins/picam/CMakeLists.txt

# ===========================================
# Find Qt Components
# ===========================================
set(QT_COMPONENTS Core Gui Widgets PrintSupport OpenGL)
if(EZSPECCAM_BUILD_TESTS)
    list(APPEND QT_COMPONENTS Test)
endif()

find_package(Qt6 6.2 COMPONENTS ${QT_COMPONENTS} REQUIRED)
if(Qt6_VERSION VERSION_LESS 6.2)
    message(FATAL_ERROR "Qt version ${Qt6_VERSION} is too old. Requires Qt 6.2 or higher.")
endif()
# Note: project originally required Qt 6.8 on Windows. Lowered to 6.2 so the
# Linux build (system Qt 6.2.4) can configure; Windows builds may now also
# use 6.2 or higher. Bump back up if a feature requires it.

# ===========================================
# Find External Dependencies
# ===========================================

# OpenGL for QCustomPlot hardware acceleration
find_package(OpenGL REQUIRED)

# QCustomPlot (bundled in gui module)
set(QCUSTOMPLOT_VERSION 2.1.1)
set(QCUSTOMPLOT_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/qcustomplot" CACHE PATH "QCustomPlot include directory")
set(QCUSTOMPLOT_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/qcustomplot/qcustomplot.cpp" CACHE PATH "QCustomPlot source file")

# ===========================================
# Compiler Setup
# ===========================================
if(MSVC)
    # MSVC-specific settings
    add_compile_options(/W4 /WX- /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _UNICODE)

    # PDB settings
    set(CMAKE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdb")

else()
    # GCC/Clang settings
    add_compile_options(-Wall -Wextra -Wpedantic)

    # Enable all warnings except deprecated
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wno-deprecated-declarations)
    endif()
endif()

# ===========================================
# Output Directories
# ===========================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib/Debug")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib/Debug")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib/Release")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib/Release")

# Create directory structure in build tree
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Debug")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/lib/Debug")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include")

# ===========================================
# Include Directory
# ===========================================
set(EZSPECCAM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
list(APPEND CMAKE_INCLUDE_PATH "${EZSPECCAM_INCLUDE_DIR}")

# ===========================================
# Export compile commands for tooling
# ===========================================
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ===========================================
# Subdirectories
# ===========================================
add_subdirectory(src/core)

if(EZSPECCAM_BUILD_APP)
    add_subdirectory(src/app)
endif()

if(EZSPECCAM_BUILD_PLUGINS)
    add_subdirectory(src/plugins)
endif()

if(EZSPECCAM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ===========================================
# Installation Rules
# ===========================================
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/"
        DESTINATION "include/ezspeccam"
        FILES_MATCHING PATTERN "*.h"
        PATTERN ".git" EXCLUDE
        PATTERN ".svn" EXCLUDE)

# CMake config files - disabled for now
# install(EXPORT EZSpecCamTargets
#         FILE EZSpecCamTargets.cmake
#         DESTINATION "lib/cmake/EZSpecCam")

# ===========================================
# Print Configuration Summary
# ===========================================
message(STATUS "")
message(STATUS "===========================================")
message(STATUS "EZSpecCam ${PROJECT_VERSION}")
message(STATUS "===========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Qt version: ${Qt6_VERSION}")
message(STATUS "Build tests: ${EZSPECCAM_BUILD_TESTS}")
message(STATUS "Build app: ${EZSPECCAM_BUILD_APP}")
message(STATUS "Build plugins: ${EZSPECCAM_BUILD_PLUGINS}")
message(STATUS "===========================================")
message(STATUS "")