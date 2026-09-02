# EZSpecCam Test Suite
add_subdirectory(test_configuration)
add_subdirectory(test_mock_driver)
add_subdirectory(test_plugin_loading)
add_subdirectory(test_app_controller)
add_subdirectory(test_view_widgets_perf)
add_subdirectory(test_postprocess_perf)

# qhyccd/hamamatsu tests depend on Windows-only vendor SDKs.
if(WIN32)
    add_subdirectory(test_qhyccd_driver)
    add_subdirectory(test_hamamatsu_driver)
endif()

add_subdirectory(test_metadata_save_load)
add_subdirectory(test_row_range_dialog)
add_subdirectory(test_spectrum_axis_range)
add_subdirectory(test_spectrum_auto_recovery)
# add_subdirectory(test_cli)
add_subdirectory(test_picam_driver)
add_subdirectory(test_app_mode)
add_subdirectory(test_plugin_loader)
