# Right-click menu for ImageViewWidget: add/remove crosshair and open the profile
# without a keyboard. Lightweight target — only the image view, not the full GUI.
add_executable(test_image_context_menu)
target_sources(test_image_context_menu PRIVATE
    test_image_context_menu.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/qcustomplot.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/Theme.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/ImageViewWidget.cpp
)
target_include_directories(test_image_context_menu PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display
)
target_compile_definitions(test_image_context_menu PRIVATE QCUSTOMPLOT_USE_OPENGL)
target_link_libraries(test_image_context_menu PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::OpenGL Qt6::PrintSupport
    OpenGL::GL
)
add_test(NAME test_image_context_menu COMMAND test_image_context_menu)
