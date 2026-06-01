# Test QHYCCD driver
add_executable(test_qhyccd_driver_QHY268m)
target_sources(test_qhyccd_driver_QHY268m PRIVATE
    test_qhyccd_driver_QHY268m.cpp
    ${CMAKE_SOURCE_DIR}/src/plugins/qhyccd/QHYCCDDriver.cpp
)
target_link_libraries(test_qhyccd_driver_QHY268m PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
    "${CMAKE_SOURCE_DIR}/src/plugins/qhyccd/sdk/winlib/x64/qhyccd.lib"
)
target_include_directories(test_qhyccd_driver_QHY268m PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/plugins/qhyccd
    "${CMAKE_SOURCE_DIR}/src/plugins/qhyccd/sdk/include"
)
set_target_properties(test_qhyccd_driver_QHY268m PROPERTIES
    Qt6::MinimalQt6 TRUE
)
# add_test(NAME test_qhyccd_driver_QHY268m COMMAND test_qhyccd_driver_QHY268m)