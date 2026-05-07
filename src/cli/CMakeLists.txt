# CLI application
qt_add_executable(ezspeccam_cli)

target_link_libraries(ezspeccam_cli PRIVATE
    ezspeccam_core
    Qt6::Core
)

target_sources(ezspeccam_cli PRIVATE
    main.cpp
    CommandLineParser.h CommandLineParser.cpp
    CaptureController.h CaptureController.cpp
)
