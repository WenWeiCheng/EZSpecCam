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
