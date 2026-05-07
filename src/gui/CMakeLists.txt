# GUI application
qt_add_executable(ezspeccam_gui)

target_link_libraries(ezspeccam_gui PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::PrintSupport Qt6::OpenGL
    OpenGL::GL
)

target_compile_definitions(ezspeccam_gui PRIVATE QCUSTOMPLOT_USE_OPENGL)

target_sources(ezspeccam_gui PRIVATE
    main.cpp
    AppController.h AppController.cpp
    widgets/MainWindow.h widgets/MainWindow.cpp
    ui/MainWindowUi.h ui/MainWindowUi.cpp
    widgets/CameraTab.h widgets/CameraTab.cpp
    widgets/ImageViewWidget.h widgets/ImageViewWidget.cpp
    widgets/SpectrumViewWidget.h widgets/SpectrumViewWidget.cpp
    qcustomplot.h qcustomplot.cpp
    widgets/LoadingIndicator.h widgets/LoadingIndicator.cpp
    widgets/ParameterWidgetFactory.h widgets/ParameterWidgetFactory.cpp
    widgets/StatisticsDialog.h widgets/StatisticsDialog.cpp
    widgets/CustomRangeDialog.h widgets/CustomRangeDialog.cpp
    widgets/RowRangeDialog.h widgets/RowRangeDialog.cpp
    widgets/DataTab.h widgets/DataTab.cpp
    widgets/PluginTab.h widgets/PluginTab.cpp
    widgets/CameraConfigDialogUi.h widgets/CameraConfigDialogUi.cpp
    widgets/CameraConfigDialog.h widgets/CameraConfigDialog.cpp
)