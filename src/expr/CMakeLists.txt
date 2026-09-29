add_library(lumina_expr STATIC
    Expression.h
    ExpressionInternal.h
    Expression.cpp          # Engine, cache e ponte com core
    ExpressionLexer.cpp     # tokenizer
    ExpressionParser.cpp    # arvore sintatica
    ExpressionEval.cpp      # interpretador
)

add_library(lumina::expr ALIAS lumina_expr)

target_link_libraries(lumina_expr PUBLIC lumina::core)
target_link_libraries(lumina_expr PRIVATE Qt6::Core)

set_target_properties(lumina_expr PROPERTIES FOLDER "Lumina/Core")
