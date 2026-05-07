# Mock camera driver plugin
qt_add_plugin(mock_camera_driver MODULE)

target_link_libraries(mock_camera_driver PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Widgets
)

target_include_directories(mock_camera_driver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_sources(mock_camera_driver PRIVATE
    MockCameraDriver.h MockCameraDriver.cpp
)

# Install mock.json as plugin metadata
target_sources(mock_camera_driver PRIVATE
    mock.json
)

# Copy plugin DLL to the plugins/drivers directory next to the GUI executable
add_custom_command(TARGET mock_camera_driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:ezspeccam_gui>/plugins/drivers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:mock_camera_driver>"
        "$<TARGET_FILE_DIR:ezspeccam_gui>/plugins/drivers/"
    COMMENT "Deploying mock_camera_driver to plugins/drivers/"
)
