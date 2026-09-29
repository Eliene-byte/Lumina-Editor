add_library(lumina_io STATIC
    ProjectSerializer.h  ProjectSerializer.cpp
    JsonHelpers.h        JsonHelpers.cpp
)

add_library(lumina::io ALIAS lumina_io)

target_link_libraries(lumina_io
    PUBLIC
        lumina::core
    PRIVATE
        Qt6::Core
        Qt6::Gui
)

set_target_properties(lumina_io PROPERTIES FOLDER "Lumina/IO")
