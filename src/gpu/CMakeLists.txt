add_library(lumina_gpu STATIC
    GLContext.h           GLContext.cpp
    ShaderProgram.h       ShaderProgram.cpp
    ShaderLoader.h        ShaderLoader.cpp
    NodeUniforms.h        NodeUniforms.cpp
    RenderTarget.h        RenderTarget.cpp
    Compositor.h          Compositor.cpp
)

add_library(lumina::gpu ALIAS lumina_gpu)

target_link_libraries(lumina_gpu
    PUBLIC
        lumina::core
        lumina::resources
        Qt6::Gui
        Qt6::OpenGL
    PRIVATE
        Qt6::Core
)

set_target_properties(lumina_gpu PROPERTIES FOLDER "Lumina/GPU")
