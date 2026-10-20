# Despike 参数窗口：只剩主阈值一个参数，改动通过 paramsChanged 通知外层
add_executable(test_despike_dialog)
target_sources(test_despike_dialog PRIVATE
    test_despike_dialog.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/dialogs/DespikeDialog.cpp
)
target_link_libraries(test_despike_dialog PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)
target_include_directories(test_despike_dialog PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/gui/widgets
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/dialogs
)
set_target_properties(test_despike_dialog PROPERTIES
    Qt6::MinimalQt6 TRUE
)
add_test(NAME test_despike_dialog COMMAND test_despike_dialog)
