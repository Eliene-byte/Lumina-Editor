cmake_minimum_required(VERSION 3.28)

# Reproduz so a parte do configure que NAO depende de compilador: os dois
# find_package. Em modo script (-P) o CMake resolve pacotes de configuracao
# igual, entao um erro aqui e o mesmo erro que derruba "Configurar" no CI.
# A vantagem e que a mensagem sai na tela, sem precisar de log do Actions.

set(QT_ROOT "$ENV{LUMINA_QT_ROOT}")
set(FFMPEG_ROOT "$ENV{LUMINA_FFMPEG_ROOT}")

if(NOT QT_ROOT)
    message(FATAL_ERROR "defina LUMINA_QT_ROOT")
endif()

list(APPEND CMAKE_PREFIX_PATH "${QT_ROOT}")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

message(STATUS "--- Qt 6 ---")
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    OpenGL
    OpenGLWidgets
)
message(STATUS "Qt ${Qt6_VERSION} ok, Qt6_DIR=${Qt6_DIR}")

# O alvo que a regra qt-module do guard exige. Se o pacote provides nao
# declara este target, o erro real e "target Qt6::OpenGLWidgets nao existe".
foreach(t Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets)
    if(TARGET ${t})
        message(STATUS "  target ${t}: ok")
    else()
        message(SEND_ERROR "  target ${t}: NAO EXISTE apos o find_package")
    endif()
endforeach()

message(STATUS "--- FFmpeg ---")
if(NOT FFMPEG_ROOT)
    message(FATAL_ERROR "defina LUMINA_FFMPEG_ROOT")
endif()
find_package(FFmpeg REQUIRED)
message(STATUS "FFmpeg: avcodec=${FFmpeg_avcodec_LIBRARY}")
message(STATUS "FFmpeg: avformat=${FFmpeg_avformat_LIBRARY}")
message(STATUS "FFmpeg: avutil=${FFmpeg_avutil_LIBRARY}")
message(STATUS "FFmpeg: swscale=${FFmpeg_swscale_LIBRARY}")
message(STATUS "FFmpeg: swresample=${FFmpeg_swresample_LIBRARY}")
