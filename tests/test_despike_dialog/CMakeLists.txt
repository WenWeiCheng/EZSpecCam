# Despike 参数窗口：只暴露主阈值/次阈值/均值窗口半宽三个参数，
# 改动即时作用到 SpectrumViewWidget 显示的谱线上
add_executable(test_despike_dialog)
target_sources(test_despike_dialog PRIVATE
    test_despike_dialog.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/Theme.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/qcustomplot.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/PostProcess.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/SpectrumViewWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/dialogs/DespikeDialog.cpp
)
target_link_libraries(test_despike_dialog PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::OpenGL Qt6::PrintSupport
    OpenGL::GL
)
target_compile_definitions(test_despike_dialog PRIVATE
    QCUSTOMPLOT_USE_OPENGL
)
target_include_directories(test_despike_dialog PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/gui/widgets
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/dialogs
)
set_target_properties(test_despike_dialog PROPERTIES
    Qt6::MinimalQt6 TRUE
)
add_test(NAME test_despike_dialog COMMAND test_despike_dialog)
