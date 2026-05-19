# GUI application
find_package(OpenMP REQUIRED)
qt_add_executable(ezspeccam_gui)

target_link_libraries(ezspeccam_gui PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::PrintSupport Qt6::OpenGL
    OpenMP::OpenMP_CXX
    OpenGL::GL
)

target_compile_definitions(ezspeccam_gui PRIVATE QCUSTOMPLOT_USE_OPENGL)

target_sources(ezspeccam_gui PRIVATE
    main.cpp
    AppController.h AppController.cpp
    widgets/MainWindow.h widgets/MainWindow.cpp
    widgets/PostProcess.h widgets/PostProcess.cpp
    ui/MainWindowUi.h ui/MainWindowUi.cpp
    qcustomplot.h qcustomplot.cpp
    widgets/config/CameraTab.h widgets/config/CameraTab.cpp
    ui/CameraTabUi.h ui/CameraTabUi.cpp
    widgets/config/LoadingIndicator.h widgets/config/LoadingIndicator.cpp
    widgets/config/ParameterWidgetFactory.h widgets/config/ParameterWidgetFactory.cpp
    widgets/config/DataTab.h widgets/config/DataTab.cpp
    ui/DataTabUi.h ui/DataTabUi.cpp
    widgets/config/PluginTab.h widgets/config/PluginTab.cpp
    ui/CameraConfigDialogUi.h ui/CameraConfigDialogUi.cpp
    widgets/config/CameraConfigDialog.h widgets/config/CameraConfigDialog.cpp
    widgets/display/ImageViewWidget.h widgets/display/ImageViewWidget.cpp
    widgets/display/SpectrumViewWidget.h widgets/display/SpectrumViewWidget.cpp
    widgets/display/StatisticsDialog.h widgets/display/StatisticsDialog.cpp
    widgets/display/ProfileWindow.h widgets/display/ProfileWindow.cpp
    widgets/dialogs/CustomRangeDialog.h widgets/dialogs/CustomRangeDialog.cpp
    widgets/dialogs/RowRangeDialog.h widgets/dialogs/RowRangeDialog.cpp
    widgets/dialogs/ScaleControlDialog.h widgets/dialogs/ScaleControlDialog.cpp
    workers/FileSaverWorker.h workers/FileSaverWorker.cpp
    workers/SaveTypes.h
    workers/IImageFormatHandler.h
    workers/formats/CsvFormatHandler.h workers/formats/CsvFormatHandler.cpp
    workers/formats/TiffFormatHandler.h workers/formats/TiffFormatHandler.cpp
)
