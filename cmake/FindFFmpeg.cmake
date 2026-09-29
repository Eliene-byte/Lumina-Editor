# FindFFmpeg.cmake
#
# Localiza um build do FFmpeg (preferencialmente LGPL, 6.0+) e expoe os
# componentes como targets importados:
#
#   FFmpeg::avutil  FFmpeg::avcodec  FFmpeg::avformat
#   FFmpeg::swscale FFmpeg::swresample
#
# Ordem de busca:
#   1. -DFFMPEG_ROOT=<caminho>  ou a variavel de ambiente FFMPEG_ROOT
#   2. pkg-config (Linux/macOS)
#   3. CMAKE_PREFIX_PATH
#   4. Registros do sistema (Windows)
#
# Se LUMINA_ENABLE_FFMPEG=OFF, este modulo apenas define os targets vazios e o
# projeto compila com os stubs de midia (util para desenvolvimento rapido).

include(FindPackageHandleStandardArgs)

if(FFMPEG_ROOT)
    set(_lmn_ff_root "${FFMPEG_ROOT}")
elseif(DEFINED ENV{FFMPEG_ROOT})
    set(_lmn_ff_root "$ENV{FFMPEG_ROOT}")
else()
    set(_lmn_ff_root "")
endif()

# --- Componentes que precisamos ------------------------------------------------
set(_lmn_ff_libs avutil avcodec avformat swscale swresample)
set(_lmn_ff_pkgs libavutil libavcodec libavformat libswscale libswresample)

set(FFMPEG_FOUND FALSE)
set(FFMPEG_VERSION "")
set(FFMPEG_INCLUDE_DIRS "")
set(FFMPEG_LIBRARY_DIRS "")

# --- 1) pkg-config --------------------------------------------------------------
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND AND NOT FFMPEG_FOUND)
    pkg_check_modules(PC_FFMPEG QUIET ${_lmn_ff_pkgs})
    if(PC_FFMPEG_FOUND)
        set(FFMPEG_FOUND TRUE)
        set(FFMPEG_VERSION "${PC_FFMPEG_VERSION}")
        set(FFMPEG_INCLUDE_DIRS "${PC_FFMPEG_INCLUDE_DIRS}")
        set(FFMPEG_LIBRARY_DIRS "${PC_FFMPEG_LIBRARY_DIRS}")
    endif()
endif()

# --- 2) Busca manual (necessaria no Windows com binarios pre-compilados) --------
if(NOT FFMPEG_FOUND)
    set(_lmn_ff_hints
        "${_lmn_ff_root}"
        "$ENV{LIB}/ffmpeg"
        "$ENV{INCLUDE}/ffmpeg"
        "C:/ffmpeg"
        "C:/ffmpeg-dev"
    )

    find_path(FFMPEG_INCLUDE_DIR
        NAMES libavcodec/avcodec.h
        HINTS ${_lmn_ff_hints}
        PATH_SUFFIXES include ffmpeg include/ffmpeg
        DOC "Diretorio que contem libavcodec/avcodec.h"
    )

    if(FFMPEG_INCLUDE_DIR)
        # Numero da versao vem de libavutil/avutil.h (FF_API_ macros) ou version.h
        if(EXISTS "${FFMPEG_INCLUDE_DIR}/libavutil/version.h")
            file(STRINGS "${FFMPEG_INCLUDE_DIR}/libavutil/version.h" _ver_major
                 REGEX "^#define[ \t]+LIBAVUTIL_VERSION_MAJOR[ \t]+([0-9]+)")
            file(STRINGS "${FFMPEG_INCLUDE_DIR}/libavutil/version.h" _ver_minor
                 REGEX "^#define[ \t]+LIBAVUTIL_VERSION_MINOR[ \t]+([0-9]+)")
            string(REGEX MATCH "[0-9]+" _ver_major "${_ver_major}")
            string(REGEX MATCH "[0-9]+" _ver_minor "${_ver_minor}")
            if(_ver_major)
                set(FFMPEG_VERSION "${_ver_major}.${_ver_minor}")
            endif()
        endif()

        set(_lmn_ff_hints_lib
            "${_lmn_ff_root}"
            "$ENV{LIB}"
            "C:/ffmpeg/lib"
            "C:/ffmpeg-dev/lib"
        )

        set(_lmn_ff_all_found TRUE)
        foreach(_lib IN LISTS _lmn_ff_libs)
            string(TOUPPER "${_lib}" _lib_upper)
            find_library(FFMPEG_${_lib_upper}_LIBRARY
                NAMES ${_lib}
                HINTS ${_lmn_ff_hints_lib}
                PATH_SUFFIXES lib bin lib/x64
            )
            if(NOT FFMPEG_${_lib_upper}_LIBRARY)
                set(_lmn_ff_all_found FALSE)
            endif()
            list(APPEND FFMPEG_LIBRARY_DIRS "${FFMPEG_${_lib_upper}_LIBRARY}")
        endforeach()

        if(_lmn_ff_all_found)
            set(FFMPEG_FOUND TRUE)
            list(REMOVE_DUPLICATES FFMPEG_LIBRARY_DIRS)
            get_filename_component(FFMPEG_INCLUDE_DIRS "${FFMPEG_INCLUDE_DIR}" ABSOLUTE)
        endif()
    endif()
endif()

find_package_handle_standard_args(FFmpeg
    REQUIRED_VARS FFMPEG_INCLUDE_DIRS FFMPEG_LIBRARY_DIRS
    VERSION_VAR FFMPEG_VERSION
)

if(FFMPEG_FOUND AND NOT TARGET FFmpeg::avutil)
    add_library(FFmpeg::avutil UNKNOWN IMPORTED)
    set_target_properties(FFmpeg::avutil PROPERTIES
        IMPORTED_LOCATION "${FFMPEG_AVUTIL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
    )

    foreach(_comp IN ITEMS avcodec avformat swscale swresample)
        string(TOUPPER "${_comp}" _comp_upper)
        add_library(FFmpeg::${_comp} UNKNOWN IMPORTED)
        set_target_properties(FFmpeg::${_comp} PROPERTIES
            IMPORTED_LOCATION "${FFMPEG_${_comp_upper}_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
        )
        if(UNIX)
            set_property(TARGET FFmpeg::${_comp} APPEND PROPERTY
                INTERFACE_LINK_LIBRARIES FFmpeg::avutil)
        endif()
    endforeach()
endif()

mark_as_advanced(FFMPEG_INCLUDE_DIR FFMPEG_LIBRARY_DIRS
    FFMPEG_AVUTIL_LIBRARY FFMPEG_AVCODEC_LIBRARY FFMPEG_AVFORMAT_LIBRARY
    FFMPEG_SWSCALE_LIBRARY FFMPEG_SWRESAMPLE_LIBRARY)

# --- Modo sem FFmpeg: cria stubs para o build nao quebrar ---------------------
if(NOT FFMPEG_FOUND AND NOT LUMINA_ENABLE_FFMPEG)
    message(STATUS "FFmpeg desabilitado: usando stubs de midia")
    foreach(_comp IN ITEMS avutil avcodec avformat swscale swresample)
        if(NOT TARGET FFmpeg::${_comp})
            add_library(FFmpeg::${_comp} INTERFACE IMPORTED)
        endif()
    endforeach()
endif()
