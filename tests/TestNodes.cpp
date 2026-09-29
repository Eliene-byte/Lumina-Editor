// TestNodes.cpp - Catalogo de nos e Contratos que todos precisam cumprir.
//
// Um no registrado e usado por tres consumidores ao mesmo tempo: a UI (que le
// o desc), o compositor (que liga o shader) e a serializacao (que grava o
// desc). Estes testes verificam que os tres concordam, e que nenhum no quebra
// os invariantes que o motor assume.
#include "TestSupport.h"

#include <algorithm>
#include <set>

#include <QFile>

#include "core/NodeRegistry.h"
#include "core/Project.h"
#include "gpu/NodeUniforms.h"
#include "nodes/Nodes.h"

using namespace lmn;
using namespace lmn::nodes;

namespace {

// O caminho do shader no desc precisa existir no recurso Qt. Sem isso, o
// programa compila mas some um retangulo preto em tempo de execucao.
bool shaderExists(const NodeTypeDesc& desc) {
    if (desc.shader.empty()) return true;
    // O prefixo :/ e a raiz do recurso; os shaders vivem em :/shaders.
    const std::string path = desc.shader;
    const size_t slash = path.find_last_of('/');
    const std::string file = slash == std::string::npos ? path : path.substr(slash + 1);
    return QFile::exists(QStringLiteral(":/shaders/") + QString::fromStdString(file));
}

}  // namespace

LUMINA_TEST(Nodes, catalogoVazio) {
    const int before = registeredNodeTypeCount();
    CHECK(before > 0);
    CHECK_EQ(NodeRegistry::instance().allTypes().size(), before);
}

LUMINA_TEST(Nodes, idsUnicos) {
    std::set<std::string> seen;
    for (const NodeTypeDesc* desc : NodeRegistry::instance().allTypes()) {
        const bool inserted = seen.insert(desc->type).second;
        // Registro com id repetido sobrescreve o anterior em silencio, e o
        // primeiro some da biblioteca sem aviso.
        CHECK(inserted);
    }
}

LUMINA_TEST(Nodes, metadadosPreenchidos) {
    for (const NodeTypeDesc* desc : NodeRegistry::instance().allTypes()) {
        CHECK(!desc->type.empty());
        CHECK(!desc->displayName.empty());
        CHECK(!desc->category.empty());
    }
}

LUMINA_TEST(Nodes, shadersExistem) {
    // O teste que mais pega erro real: um caminho de shader errado nao falha
    // na compilacao, falha como "no nao fez nada" na hora de renderizar.
    for (const NodeTypeDesc* desc : NodeRegistry::instance().allTypes()) {
        if (!shaderExists(*desc)) {
            ::lmn::test::fail(__FILE__, __LINE__,
                              "shader inexistente para " + desc->type + ": " +
                                  desc->shader);
        }
    }
}

LUMINA_TEST(Nodes, coerenciaDeEntradas) {
    for (const NodeTypeDesc* desc : NodeRegistry::instance().allTypes()) {
        const int inputs = desc->inputCount();

        // As flags precisam concordar com o numero de entradas declaradas.
        CHECK_EQ(desc->isGenerator, inputs == 0);
        CHECK_EQ(desc->isFilter, inputs == 1);
        CHECK_EQ(desc->isMerger, inputs > 1);

        // Um Merge sem backdrop nao faz sentido: sem entrada 1 nao ha o que
        // mesclar, e o shader cairia no caso "so frente".
        if (desc->isMerger) CHECK(inputs >= 2);

        // Toda saida nomeada.
        CHECK(desc->outputCount() >= 1);
    }
}

LUMINA_TEST(Nodes, propriedadesCoerentes) {
    for (const NodeTypeDesc* desc : NodeRegistry::instance().allTypes()) {
        std::set<std::string> names;
        for (const auto& spec : desc->properties) {
            CHECK(!spec.name.empty());
            CHECK(names.insert(spec.name).second);
            CHECK(spec.min <= spec.max);
            // O padrao tem de estar dentro da faixa, senao o controle comeca
            // contradizendo a si mesmo.
            if (spec.type == ValueType::Double) {
                CHECK(spec.defaultValue.asDouble() >= spec.min - 1e-6);
                CHECK(spec.defaultValue.asDouble() <= spec.max + 1e-6);
            }
            // Enum precisa de opcoes.
            if (spec.ui == PropertyUi::Enum) CHECK(!spec.options.empty());
        }
    }
}

LUMINA_TEST(Nodes, binderRegistradoParaTodoTipo) {
    // Sem binder, o compositor cai no caminho generico que so publica Double,
    // Bool, Color e vetores: um no com parametro de texto (nomes de enum,
    // caminhos de arquivo) perderia o valor silenciosamente.
    for (const NodeTypeDesc* desc : NodeRegistry::instance().allTypes()) {
        CHECK(gpu::NodeUniformRegistry::instance().has(desc->type));
    }
}

LUMINA_TEST(Nodes, instanciacaoCompleta) {
    // Todo tipo tem de instanciar sem quebrar: o registro cria as propriedades
    // a partir do spec, e um spec invalido apareceria aqui.
    for (const NodeTypeDesc* desc : NodeRegistry::instance().allTypes()) {
        Node node = NodeRegistry::instance().create(desc->type, 1);
        CHECK_EQ(node.type(), desc->type);
        CHECK_EQ(node.inputCount(), desc->inputCount());

        // As propriedades do spec tem de existir no no criado.
        for (const auto& spec : desc->properties) {
            const Property* prop = node.findProperty(spec.name);
            if (prop == nullptr) {
                ::lmn::test::fail(__FILE__, __LINE__,
                                  desc->type + " nao criou a propriedade " + spec.name);
            }
            CHECK(prop != nullptr);
        }
    }
}

LUMINA_TEST(Nodes, propriedadesTransform) {
    // Nos com hasTransform precisam ter as propriedades de transformacao.
    const auto hasProp = [](const Node& node, const char* name) {
        return node.findProperty(name) != nullptr;
    };

    for (const NodeTypeDesc* desc : NodeRegistry::instance().allTypes()) {
        if (!desc->hasTransform) continue;
        Node node = NodeRegistry::instance().create(desc->type, 1);
        CHECK(hasProp(node, "position"));
        CHECK(hasProp(node, "scale"));
        CHECK(hasProp(node, "rotation"));
        CHECK(hasProp(node, "opacity"));
    }
}

LUMINA_TEST(Nodes, categoriasEsperadas) {
    // As categorias sao a organizacao da biblioteca na interface. As cinco
    // basicas precisam existir.
    const auto categories = NodeRegistry::instance().categories();
    const auto has = [&categories](const char* name) {
        return std::find(categories.begin(), categories.end(), name) != categories.end();
    };
    CHECK(has("Fonte"));
    CHECK(has("Cor"));
    CHECK(has("Filtro"));
    CHECK(has("Mesclagem"));
    CHECK(has("Tempo"));
}

LUMINA_TEST(Nodes, tiposEssenciais) {
    // Um grafo novo depende destes: sem um Merge nao ha como juntar nada, e
    // sem um Solido nao ha imagem inicial.
    CHECK(NodeRegistry::instance().has("src.solid"));
    CHECK(NodeRegistry::instance().has("merge.over"));
    CHECK(NodeRegistry::instance().has("color.viewTransform"));
    CHECK(NodeRegistry::instance().has("fx.blur"));
    CHECK(NodeRegistry::instance().has("cc.brightnessContrast"));
}

LUMINA_TEST(Nodes, grafoCompletoRenderizaNaFita) {
    // Monta o grafo tipico de uma cena e confere que ele e valido: e o mesmo
    // caminho que createDefaultComposition usa, entao um erro aqui aparece
    // como tela preta no primeiro programa.
    Project project;
    project.createDefaultComposition();
    Composition* comp = project.composition(project.compositions().begin()->first);
    CHECK(comp != nullptr);
    if (!comp) return;

    std::string error;
    CHECK(comp->graph().validate(&error));
    CHECK(error.empty());
    CHECK(comp->graph().isAcyclic());
    CHECK(comp->graph().output() != kInvalidId);
    CHECK(comp->graph().topologicalOrder().size() == comp->graph().size());
}

LUMINA_TEST(Nodes, grafoLongoEmOrdemTopologica) {
    // Cadeia de 30 nos: o avalia tem de ser linear e sem ciclo, que e o caso
    // que estouraria a pilha de recursao.
    Project project;
    const CompId id = project.createComposition("Cadeia", Size{640, 480}, 30.0, 5.0);
    Composition* comp = project.composition(id);
    if (!comp) return;

    NodeId previous = kInvalidId;
    for (int i = 0; i < 30; ++i) {
        Node* node = comp->graph().createNode("cc.brightnessContrast",
                                             "N" + std::to_string(i));
        CHECK(node != nullptr);
        if (!node) return;
        if (previous != kInvalidId) comp->graph().connect(previous, 0, node->id(), 0);
        previous = node->id();
    }
    comp->graph().setOutput(previous);

    CHECK(comp->graph().isAcyclic());
    const auto order = comp->graph().topologicalOrder();
    CHECK_EQ(order.size(), size_t(30));
    CHECK(order.front() == comp->graph().nodeIds().front());
    CHECK(order.back() == previous);
}
