cmake_minimum_required(VERSION 3.28)

project(Lumina
    VERSION 0.1.0
    DESCRIPTION "Editor de video portátil: composicao node-based, timeline multipista e color grading"
    HOMEPAGE_URL "https://github.com/Eliene-byte/Lumina-Editor"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Opcoes
# ---------------------------------------------------------------------------
option(LUMINA_BUILD_TESTS     "Compila os testes unitarios"            ON)
option(LUMINA_ENABLE_FFMPEG  "Habilita decode/encode via FFmpeg"      ON)
option(LUMINA_SHIPPING       "Otimiza para distribuicao (LTO, sem debug)" OFF)
option(LUMINA_BUILD_APP      "Compila o executavel da aplicacao"      ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Compilacao
# ---------------------------------------------------------------------------
add_library(lumina_flags INTERFACE)
add_library(lumina::flags ALIAS lumina_flags)

if(MSVC)
    target_compile_options(lumina_flags INTERFACE
        /permissive- /Zc:__cplusplus /Zc:preprocessor /utf-8 /W4 /wd4100 /wd4996
        /fp:fast /MP /bigobj
    )
    target_compile_definitions(lumina_flags INTERFACE
        NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS
        UNICODE _UNICODE
    )
else()
    target_compile_options(lumina_flags INTERFACE
        -Wall -Wextra -Wno-unused-parameter
    )
endif()

if(LUMINA_SHIPPING AND CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(lumina_flags INTERFACE $<$<CXX_COMPILER_ID:MSVC>:/GL>)
    target_compile_options(lumina_flags INTERFACE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-flto>)
    target_compile_definitions(lumina_flags INTERFACE LUMINA_SHIPPING=1)
endif()

# ---------------------------------------------------------------------------
# Dependencias
# ---------------------------------------------------------------------------
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets OpenGL)
qt_standard_project_setup(REQUIRES 6.5)

if(APPLE)
    find_package(Qt6 6.5 REQUIRED COMPONENTS OpenGLWidgets)
endif()

if(LUMINA_ENABLE_FFMPEG)
    find_package(FFmpeg REQUIRED)
endif()

# ---------------------------------------------------------------------------
# Fontes de shaders: compiladas em um recurso Qt para nao depender de disco
# ---------------------------------------------------------------------------
add_subdirectory(resources)

# ---------------------------------------------------------------------------
# Modulos do nucleo
# ---------------------------------------------------------------------------
add_subdirectory(src/core)
add_subdirectory(src/expr)
add_subdirectory(src/gpu)
add_subdirectory(src/nodes)
add_subdirectory(src/media)
add_subdirectory(src/io)
add_subdirectory(src/ui)

if(LUMINA_BUILD_APP)
    add_subdirectory(src/app)
endif()

if(LUMINA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Instalacao / pacote portatil
# ---------------------------------------------------------------------------
include(Packaging)
lumina_configure_packaging()
