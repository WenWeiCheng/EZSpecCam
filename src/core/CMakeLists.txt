# EZSpecCam Core Module
# Static library for camera interfaces, data, and config

add_library(ezspeccam_core STATIC)

target_include_directories(ezspeccam_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/ezspeccam>
)

target_sources(ezspeccam_core PRIVATE
    ICameraDriver.h
    CameraTypes.h
)

target_link_libraries(ezspeccam_core PUBLIC Qt6::Core Qt6::Gui)

add_library(EZSpecCam::Core ALIAS ezspeccam_core)
