add_executable(lumina
    main.cpp
    HeadlessRenderer.h
    HeadlessRenderer.cpp
)

target_link_libraries(lumina
    PRIVATE
        lumina::ui
        lumina::media_ui
        lumina::expr
        lumina::nodes
        Qt6::Widgets
        Qt6::Gui
        Qt6::Core
)

set_target_properties(lumina PROPERTIES
    AUTOMOC ON
    FOLDER "Lumina/App"
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
