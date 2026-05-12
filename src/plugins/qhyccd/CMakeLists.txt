# QHYCCD camera driver plugin
qt_add_plugin(qhyccd_camera_driver MODULE)

target_link_libraries(qhyccd_camera_driver PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Widgets
    "${CMAKE_SOURCE_DIR}/src/plugins/qhyccd/sdk/winlib/x64/qhyccd.lib"
)

target_include_directories(qhyccd_camera_driver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    "${CMAKE_SOURCE_DIR}/src/plugins/qhyccd/sdk/include"
)

target_sources(qhyccd_camera_driver PRIVATE
    QHYCCDDriver.h QHYCCDDriver.cpp
)

# Install qhyccd.json as plugin metadata
target_sources(qhyccd_camera_driver PRIVATE
    qhyccd.json
)

# Copy plugin DLL to the plugins/drivers directory next to the GUI executable
add_custom_command(TARGET qhyccd_camera_driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:ezspeccam_gui>/plugins/drivers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:qhyccd_camera_driver>"
        "$<TARGET_FILE_DIR:ezspeccam_gui>/plugins/drivers/"
    COMMENT "Deploying qhyccd_camera_driver to plugins/drivers/"
)