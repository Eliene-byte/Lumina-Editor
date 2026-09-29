# Testes do Lumina.
#
# Os testes nao usam Qt Test: eles rodam em um processo sem display (o CI nao
# tem servidor X nem GPU dedicada), e QTest exige QApplication. O runner
# proprio tem ~80 linhas e nenhuma dependencia a mais.
#
# O alvo inclui a biblioteca de recursos para que :/shaders exista: os testes
# de shader abrem esses arquivos, e sem o recurso compilado eles pulariam em
# silencio - que e o pior jeito de um teste passar.
add_executable(lumina_tests
    TestMain.cpp
    TestSupport.h
    TestCore.cpp
    TestExpression.cpp
    TestSerialization.cpp
    TestNodes.cpp
    TestShaders.cpp
    TestGpu.cpp
)

target_link_libraries(lumina_tests
    PRIVATE
        lumina::core
        lumina::expr
        lumina::io
        lumina::nodes
        lumina::gpu
        lumina::media
        lumina::resources
        Qt6::Core
        Qt6::Gui
        Qt6::OpenGL
)

set_target_properties(lumina_tests PROPERTIES
    AUTOMOC ON
    FOLDER "Lumina/Tests"
)

add_test(NAME lumina_tests COMMAND lumina_tests)

# O timeout existe porque o teste de GPU pode travar em um driver ruim, e um CI
# parado e pior do que um CI vermelho.
set_tests_properties(lumina_tests PROPERTIES
    TIMEOUT 180
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Uma suite separada para o que precisa de GPU de verdade, para poder rodar em
# um job dedicado do CI com Mesa llvmpipe.
add_test(NAME lumina_tests_gpu COMMAND lumina_tests --filter Gpu)
set_tests_properties(lumina_tests_gpu PROPERTIES
    TIMEOUT 300
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LUMINA_REQUIRE_GPU=1"
)
