add_library(lumina_resources STATIC resources.qrc)

add_library(lumina::resources ALIAS lumina_resources)

set_target_properties(lumina_resources PROPERTIES
    OUTPUT_NAME "lumina_res"
    FOLDER "Lumina/Resources"
)

# Os shaders entram no binario; o executavel portatil nao depende de arquivos
# ao lado. O prefixo /shaders e o que ShaderLoader procura.
target_compile_definitions(lumina_resources PRIVATE
    LUMINA_SHADERS_DIR=":/shaders"
)
