# EZSpecCam CLI Application
# Headless camera control via command-line interface

add_executable(ezspeccam_cli)

target_include_directories(ezspeccam_cli PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/app
)

target_link_libraries(ezspeccam_cli PRIVATE
    EZSpecCam::Core
    Qt6::Core
)

target_sources(ezspeccam_cli PRIVATE
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/app/SequenceRunner.h
    ${CMAKE_SOURCE_DIR}/src/app/SequenceRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/app/HeadlessController.h
    ${CMAKE_SOURCE_DIR}/src/app/HeadlessController.cpp
    ${CMAKE_SOURCE_DIR}/src/app/PluginLoader.h
    ${CMAKE_SOURCE_DIR}/src/app/PluginLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/app/WaitStabilizer.h
    ${CMAKE_SOURCE_DIR}/src/app/WaitStabilizer.cpp
    ${CMAKE_SOURCE_DIR}/src/app/MessageHandler.h
    ${CMAKE_SOURCE_DIR}/src/app/MessageHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/app/formats/FrameWriter.h
    ${CMAKE_SOURCE_DIR}/src/app/formats/FrameWriter.cpp
    ${CMAKE_SOURCE_DIR}/src/app/formats/IImageFormatHandler.h
    ${CMAKE_SOURCE_DIR}/src/app/formats/TiffFormatHandler.h
    ${CMAKE_SOURCE_DIR}/src/app/formats/TiffFormatHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/app/formats/CsvFormatHandler.h
    ${CMAKE_SOURCE_DIR}/src/app/formats/CsvFormatHandler.cpp
)

# Deploy mock plugin for CLI testing
add_custom_command(TARGET ezspeccam_cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:ezspeccam_cli>/plugins/drivers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:mock_camera_driver>"
        "$<TARGET_FILE_DIR:ezspeccam_cli>/plugins/drivers/"
    COMMENT "Deploying mock_camera_driver for CLI"
)
