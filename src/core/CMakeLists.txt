# EZSpecCam Core Module
# Static library for camera interfaces, data, and config

add_library(ezspeccam_core STATIC)

target_include_directories(ezspeccam_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/interfaces>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/data>
        $<INSTALL_INTERFACE:include/ezspeccam>
)

target_sources(ezspeccam_core PRIVATE
    core_global.h
    # Interfaces
    interfaces/ICameraDriver.h
    interfaces/CameraTypes.h
    # Data
    data/PostProcessManager.h
    data/PostProcessManager.cpp
)

target_link_libraries(ezspeccam_core PUBLIC Qt6::Core Qt6::Gui)

if(EZSPECCAM_HAVE_EXIV2)
    target_link_libraries(ezspeccam_core PUBLIC Exiv2::exiv2lib)
    target_compile_definitions(ezspeccam_core PUBLIC EZSPECCAM_HAVE_EXIV2=1)
endif()

add_library(EZSpecCam::Core ALIAS ezspeccam_core)
