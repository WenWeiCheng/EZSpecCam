# Rubber-band zoom tests for ImageViewWidget: a selection dragged past the edge of
# the image must not push the axis range outside the image, because every readout
# (crosshair, tooltip, status bar) is derived from that range.
add_executable(test_image_zoom)
target_sources(test_image_zoom PRIVATE
    test_image_zoom.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/qcustomplot.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/Theme.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/ImageViewWidget.cpp
)
target_include_directories(test_image_zoom PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display
)
target_compile_definitions(test_image_zoom PRIVATE QCUSTOMPLOT_USE_OPENGL)
target_link_libraries(test_image_zoom PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::OpenGL Qt6::PrintSupport
    OpenGL::GL
)
add_test(NAME test_image_zoom COMMAND test_image_zoom)
