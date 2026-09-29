add_library(lumina_ui STATIC
    Theme.h                 Theme.cpp
    PlaybackController.h    PlaybackController.cpp
    ValueEditors.h          ValueEditors.cpp
    NodeEditorWidget.h      NodeEditorWidget.cpp
    NodeLibraryWidget.h     NodeLibraryWidget.cpp
    InspectorWidget.h       InspectorWidget.cpp
    TimelineWidget.h        TimelineWidget.cpp
    ViewerWidget.h          ViewerWidget.cpp
    MainWindow.h            MainWindow.cpp
)

add_library(lumina::ui ALIAS lumina_ui)

target_link_libraries(lumina_ui
    PUBLIC
        lumina::core
        lumina::expr
        lumina::gpu
        lumina::io
        lumina::media
        lumina::media_ui
        lumina::nodes
        Qt6::Widgets
        # OpenGLWidgets e separado de OpenGL no Qt 6 e e quem traz
        # QOpenGLWidget, usado pelo visualizador.
        Qt6::OpenGL
        Qt6::OpenGLWidgets
    PRIVATE
        Qt6::Core
        Qt6::Gui
)

# O moc precisa rodar sobre os headers que usam Q_OBJECT. AUTOUIC fica
# desligado de proposito: nao ha .ui neste projeto, so codigo.
set_target_properties(lumina_ui PROPERTIES
    AUTOMOC ON
    FOLDER "Lumina/UI"
)
