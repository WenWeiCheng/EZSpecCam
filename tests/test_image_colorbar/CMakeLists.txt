# Colour-scale geometry guards for ImageViewWidget.
add_executable(test_image_colorbar)
target_sources(test_image_colorbar PRIVATE
    test_image_colorbar.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/qcustomplot.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/Theme.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/ImageViewWidget.cpp
)
target_include_directories(test_image_colorbar PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display
)
target_compile_definitions(test_image_colorbar PRIVATE QCUSTOMPLOT_USE_OPENGL)
target_link_libraries(test_image_colorbar PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::OpenGL Qt6::PrintSupport
    OpenGL::GL
)
add_test(NAME test_image_colorbar COMMAND test_image_colorbar)
