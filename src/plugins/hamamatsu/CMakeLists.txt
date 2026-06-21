# Hamamatsu DCAMSDK4 camera driver plugin
qt_add_plugin(hamamatsu_camera_driver MODULE)

target_link_libraries(hamamatsu_camera_driver PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Widgets
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/winlib/x64/dcamapi.lib"
)

target_include_directories(hamamatsu_camera_driver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/include"
)

target_sources(hamamatsu_camera_driver PRIVATE
    HamamatsuDriver.h HamamatsuDriver.cpp
)

# Install hamamatsu.json as plugin metadata
target_sources(hamamatsu_camera_driver PRIVATE
    hamamatsu.json
)

# Copy plugin DLL to the plugins/drivers directory next to the unified executable
add_custom_command(TARGET hamamatsu_camera_driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:ezspeccam>/plugins/drivers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:hamamatsu_camera_driver>"
        "$<TARGET_FILE_DIR:ezspeccam>/plugins/drivers/"
    COMMENT "Deploying hamamatsu_camera_driver to plugins/drivers/"
)
