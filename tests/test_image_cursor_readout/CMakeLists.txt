# Cursor readout behaviour tests for ImageViewWidget: the tooltip must follow
# new frames even when the mouse has not moved (live mode).
add_executable(test_image_cursor_readout)
target_sources(test_image_cursor_readout PRIVATE
    test_image_cursor_readout.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/qcustomplot.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/Theme.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/ImageViewWidget.cpp
)
target_include_directories(test_image_cursor_readout PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display
)
target_compile_definitions(test_image_cursor_readout PRIVATE QCUSTOMPLOT_USE_OPENGL)
target_link_libraries(test_image_cursor_readout PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::OpenGL Qt6::PrintSupport
    OpenGL::GL
)
add_test(NAME test_image_cursor_readout COMMAND test_image_cursor_readout)
