add_library(lumina_core STATIC
    Types.h
    Value.h            Value.cpp
    Property.h         Property.cpp
    BlendMode.h        BlendMode.cpp
    Node.h             Node.cpp
    NodeRegistry.h     NodeRegistry.cpp
    NodeGraph.h        NodeGraph.cpp
    Composition.h      Composition.cpp
    Timeline.h         Timeline.cpp
    MediaPool.h        MediaPool.cpp
    Project.h          Project.cpp
)

add_library(lumina::core ALIAS lumina_core)

target_include_directories(lumina_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(lumina_core
    PUBLIC
        lumina::flags
    PRIVATE
        Qt6::Core
)

set_target_properties(lumina_core PROPERTIES
    FOLDER "Lumina/Core"
)
