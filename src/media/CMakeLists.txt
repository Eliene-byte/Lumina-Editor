add_library(lumina_media STATIC
    MediaBackend.h        MediaBackend.cpp
    SourceProviderGL.h    SourceProviderGL.cpp
    OffscreenRenderer.h   OffscreenRenderer.cpp
    FrameExporter.h       FrameExporter.cpp
)

# O dialogo de exportacao depende de Qt Widgets, que o resto da biblioteca de
# midia nao usa. Mantido aqui para que a biblioteca continue usavel em um
# processo sem interface (exportacao por linha de comando, testes).
add_library(lumina_media_ui STATIC
    ExportDialog.h        ExportDialog.cpp
)

add_library(lumina::media_ui ALIAS lumina_media_ui)

add_library(lumina::media ALIAS lumina_media)

target_link_libraries(lumina_media
    PUBLIC
        lumina::core
        lumina::gpu
        Qt6::Gui
        Qt6::OpenGL
    PRIVATE
        Qt6::Core
)

if(LUMINA_ENABLE_FFMPEG)
    target_link_libraries(lumina_media PRIVATE
        FFmpeg::avutil
        FFmpeg::avcodec
        FFmpeg::avformat
        FFmpeg::swscale
        FFmpeg::swresample
    )
    target_compile_definitions(lumina_media PRIVATE LUMINA_HAVE_FFMPEG=1)
endif()

set_target_properties(lumina_media PROPERTIES FOLDER "Lumina/Media")

target_link_libraries(lumina_media_ui
    PUBLIC
        lumina::media
    PRIVATE
        lumina::io
        Qt6::Widgets
        Qt6::Gui
        Qt6::Core
)

set_target_properties(lumina_media_ui PROPERTIES FOLDER "Lumina/Media")
