# Test QHYCCD driver
add_executable(test_qhyccd_driver)
target_sources(test_qhyccd_driver PRIVATE
    test_qhyccd_driver.cpp
    ${CMAKE_SOURCE_DIR}/src/plugins/qhyccd/QHYCCDDriver.cpp
)
target_link_libraries(test_qhyccd_driver PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
    "${CMAKE_SOURCE_DIR}/../2502-Sw-EZSpecCam/material/QHYCCD_API/sample/QtVS_Basic_Live/winlib/x64/qhyccd.lib"
)
target_include_directories(test_qhyccd_driver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/plugins/qhyccd
    "${CMAKE_SOURCE_DIR}/../2502-Sw-EZSpecCam/material/QHYCCD_API/sample/QtVS_Basic_Live/include"
)
set_target_properties(test_qhyccd_driver PROPERTIES
    Qt6::MinimalQt6 TRUE
)
# add_test(NAME test_qhyccd_driver COMMAND test_qhyccd_driver)