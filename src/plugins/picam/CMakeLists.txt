#==============================================================================
# Princeton Instruments PICam 5.x camera driver plugin
#==============================================================================
#
# SDK is located via the PicamRoot environment variable (set by the PICam
# installer). Override with -DPICAM_SDK_ROOT=<path> if needed.
#
# Expected layout (matches VS2010 sample convention):
#   ${PicamRoot}/
#   ├── Includes/
#   │   └── picam.h
#   └── Libraries/
#       ├── picam.lib      (x64 import library)
#       └── picam32.lib    (x86 import library)

option(EZSPECCAM_BUILD_PLUGIN_PICAM "Build PICam camera driver plugin" ON)
if(NOT EZSPECCAM_BUILD_PLUGIN_PICAM)
    return()
endif()

# —— Locate PICam SDK ——
#
# Priority: explicit -DPICAM_SDK_ROOT=... > PicamRoot env var

set(PICAM_SDK_ROOT "" CACHE PATH "Override PICam SDK path (default: $ENV{PicamRoot})")

if(PICAM_SDK_ROOT)
    set(_picam_root "${PICAM_SDK_ROOT}")
elseif(DEFINED ENV{PicamRoot})
    set(_picam_root "$ENV{PicamRoot}")
    set(PICAM_SDK_ROOT "${_picam_root}" CACHE PATH "PICam SDK path" FORCE)
else()
    message(FATAL_ERROR "PICam SDK not found. "
                        "The PicamRoot environment variable is not set.\n"
                        "  Install the PICam 5.x SDK, or set PicamRoot manually.\n"
                        "  Alternatively, pass -DPICAM_SDK_ROOT=<path>.")
endif()

set(PICAM_INCLUDE_DIR "${_picam_root}/Includes")
set(PICAM_LIBRARY_DIR "${_picam_root}/Libraries")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PICAM_LIB_NAME "picam.lib")
else()
    set(PICAM_LIB_NAME "picam32.lib")
endif()

if(NOT EXISTS "${PICAM_INCLUDE_DIR}/picam.h")
    message(FATAL_ERROR "picam.h not found at ${PICAM_INCLUDE_DIR}/picam.h")
endif()
if(NOT EXISTS "${PICAM_LIBRARY_DIR}/${PICAM_LIB_NAME}")
    message(FATAL_ERROR "${PICAM_LIB_NAME} not found at ${PICAM_LIBRARY_DIR}/${PICAM_LIB_NAME}")
endif()

message(STATUS "PICam SDK: ${_picam_root}")

# —— Plugin target ——

qt_add_plugin(picam_camera_driver MODULE)

target_link_libraries(picam_camera_driver PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Widgets
    "${PICAM_LIBRARY_DIR}/${PICAM_LIB_NAME}"
)

target_include_directories(picam_camera_driver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    "${PICAM_INCLUDE_DIR}"
)

target_sources(picam_camera_driver PRIVATE
    PicamDriver.h PicamDriver.cpp
    picam.json
)

# —— Post-build deploy ——

# Plugin DLL → plugins/drivers/
add_custom_command(TARGET picam_camera_driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:ezspeccam_gui>/plugins/drivers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:picam_camera_driver>"
        "$<TARGET_FILE_DIR:ezspeccam_gui>/plugins/drivers/"
    COMMENT "Deploying picam_camera_driver to plugins/drivers/"
)

# PICam runtime DLL → plugins/drivers/
set(PICAM_RUNTIME_DLL "${PICAM_LIBRARY_DIR}/picam.dll")
if(EXISTS "${PICAM_RUNTIME_DLL}")
    add_custom_command(TARGET picam_camera_driver POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PICAM_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:ezspeccam_gui>/plugins/drivers/"
        COMMENT "Deploying picam.dll to plugins/drivers/"
    )
else()
    message(WARNING "picam.dll not found at ${PICAM_RUNTIME_DLL}. "
                    "Plugin will fail to load at runtime without it.")
endif()
