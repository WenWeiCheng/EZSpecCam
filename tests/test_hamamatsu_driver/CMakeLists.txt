# Test Hamamatsu driver
add_executable(test_hamamatsu_driver_C16091-10)
target_sources(test_hamamatsu_driver_C16091-10 PRIVATE
    test_hamamatsu_driver_C16091-10.cpp
    ${CMAKE_SOURCE_DIR}/src/plugins/hamamatsu/HamamatsuDriver.cpp
)
target_link_libraries(test_hamamatsu_driver_C16091-10 PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
    "${CMAKE_SOURCE_DIR}/src/plugins/hamamatsu/sdk/winlib/x64/dcamapi.lib"
)
target_include_directories(test_hamamatsu_driver_C16091-10 PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/plugins/hamamatsu
    "${CMAKE_SOURCE_DIR}/src/plugins/hamamatsu/sdk/include"
)
set_target_properties(test_hamamatsu_driver_C16091-10 PROPERTIES
    Qt6::MinimalQt6 TRUE
)
# add_test(NAME test_hamamatsu_driver_C16091-10 COMMAND test_hamamatsu_driver_C16091-10)  # requires HAMAMATSU camera hardware
