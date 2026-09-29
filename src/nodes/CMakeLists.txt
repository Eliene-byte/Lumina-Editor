add_library(lumina_nodes STATIC
    Nodes.h            Nodes.cpp
    NodeHelpers.h      NodeHelpers.cpp
    ImageCache.h       ImageCache.cpp
    SourceNodes.cpp
    MediaNodes.cpp
    MergeNodes.cpp
    ColorNodes.cpp
    FilterNodes.cpp
    TimeNodes.cpp
)

add_library(lumina::nodes ALIAS lumina_nodes)

target_link_libraries(lumina_nodes
    PUBLIC
        lumina::core
    PRIVATE
        lumina::gpu
        lumina::resources
        Qt6::Gui
        Qt6::Core
)

set_target_properties(lumina_nodes PROPERTIES FOLDER "Lumina/Nodes")
