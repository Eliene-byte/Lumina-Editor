// TestCore.cpp - Modelo de dados: Value, Property, Node, grafo, timeline.
#include "TestSupport.h"

#include <algorithm>

#include "core/Composition.h"
#include "core/NodeGraph.h"
#include "core/NodeRegistry.h"
#include "core/Project.h"
#include "core/Timeline.h"
#include "media/MediaBackend.h"
#include "nodes/Nodes.h"

using namespace lmn;

// Helpers locais: o arquivo de teste nao deve depender de um no especifico
// estar registrado, senao um teste falha por causa da ordem de registro.
static bool shouldUseProxyForTest(const MediaItem& item, bool globalEnabled) {
    return media::shouldUseProxy(item, globalEnabled);
}

static NodeTypeDesc makeSolidDescForTest() {
    NodeTypeDesc d;
    d.type = "src.solid";
    d.displayName = "Solido";
    d.category = "Fonte";
    d.isGenerator = true;
    d.hasTransform = false;
    return d;
}

LUMINA_TEST(Value, tiposBasicos) {
    CHECK(Value(1.5).asDouble() == 1.5);
    CHECK(Value(true).asBool());
    CHECK_EQ(Value(std::string("abc")).asString(), std::string("abc"));

    const Value c(Color{0.25, 0.5, 0.75, 0.5});
    CHECK_NEAR(c.asColor().r, 0.25, 1e-9);
    CHECK_NEAR(c.asColor().a, 0.5, 1e-9);

    const Value v(Vec4{1.0, 2.0, 3.0, 4.0});
    CHECK_NEAR(v.toVec4().w, 4.0, 1e-9);
}

LUMINA_TEST(Value, coercoes) {
    // Um Double em Vec4: o que o shader precisa quando o controle passa um
    // valor unico para um parametro de cor.
    const Value d(0.5);
    const Vec4 v = d.toVec4();
    CHECK_NEAR(v.x, 0.5, 1e-9);
    CHECK_NEAR(v.w, 0.5, 1e-9);

    // Conversao para um tipo diferente.
    Value value(2.0);
    CHECK(value.convertTo(ValueType::Vec2));
    CHECK_NEAR(value.asVec2().y, 2.0, 1e-9);

    // Texto nao vira numero: "12" como texto continua texto.
    Value text(std::string("12"));
    CHECK(!text.convertTo(ValueType::Double));
    CHECK_EQ(text.type(), ValueType::String);
}

LUMINA_TEST(Value, aritmetica) {
    CHECK((Value(3.0) + Value(4.0)).asDouble() == 7.0);
    CHECK((Value(3.0) - Value(4.0)).asDouble() == -1.0);
    CHECK((Value(3.0) * Value(4.0)).asDouble() == 12.0);
    CHECK_NEAR((Value(4.0) / Value(2.0)).asDouble(), 2.0, 1e-9);

    // Divisao por zero nao pode gerar inf nem derrubar o programa.
    CHECK_NEAR((Value(1.0) / Value(0.0)).asDouble(), 0.0, 1e-9);

    // Vetor componente a componente.
    const Value a(Vec2{1.0, 2.0});
    const Value b(Vec2{0.5, 0.5});
    const Vec2 sum = (a + b).asVec2();
    CHECK_NEAR(sum.x, 1.5, 1e-9);
    CHECK_NEAR(sum.y, 2.5, 1e-9);

    // Interpolacao: e o que as keyframes fazem entre dois valores.
    const Value mixed = Value(Vec2{0.0, 0.0}).mix(Value(Vec2{10.0, 20.0}), 0.5);
    CHECK_NEAR(mixed.asVec2().x, 5.0, 1e-9);
}

LUMINA_TEST(Property, valorBaseSemAnimacao) {
    Property p(Value(0.5), PropertyUi::Slider);
    CHECK_NEAR(p.evaluate(0.0).asDouble(), 0.5, 1e-9);
    CHECK_NEAR(p.evaluate(100.0).asDouble(), 0.5, 1e-9);
    CHECK(!p.hasAnimation());
}

LUMINA_TEST(Property, interpolacaoLinear) {
    Property p(Value(0.0), PropertyUi::Slider);
    Keyframe a;
    a.time = 0.0;
    a.value = Value(0.0);
    a.interp = Interp::Linear;
    Keyframe b = a;
    b.time = 2.0;
    b.value = Value(10.0);

    p.keyframes().push_back(a);
    p.keyframes().push_back(b);

    CHECK_NEAR(p.evaluate(0.0).asDouble(), 0.0, 1e-9);
    CHECK_NEAR(p.evaluate(1.0).asDouble(), 5.0, 1e-6);
    CHECK_NEAR(p.evaluate(2.0).asDouble(), 10.0, 1e-9);

    // Fora da faixa: constante nas pontas, como no AE.
    CHECK_NEAR(p.evaluate(-5.0).asDouble(), 0.0, 1e-9);
    CHECK_NEAR(p.evaluate(5.0).asDouble(), 10.0, 1e-9);
}

LUMINA_TEST(Property, interpolacaoComSuavidade) {
    Property p(Value(0.0), PropertyUi::Slider);
    Keyframe a;
    a.time = 0.0;
    a.value = Value(0.0);
    a.easeOut = 0.5;
    Keyframe b = a;
    b.time = 1.0;
    b.value = Value(1.0);
    b.easeIn = 0.5;
    p.keyframes().push_back(a);
    p.keyframes().push_back(b);

    // Com suavidade, o meio da curva esta atras do linear: a easeOut segura o
    // inicio e a easeIn segura o fim.
    const double mid = p.evaluate(0.5).asDouble();
    CHECK(mid < 0.5);
    CHECK(mid > 0.1);
}

LUMINA_TEST(Property, degrau) {
    Property p(Value(0.0), PropertyUi::Slider);
    Keyframe a;
    a.time = 0.0;
    a.value = Value(0.0);
    a.interp = Interp::Hold;
    Keyframe b = a;
    b.time = 1.0;
    b.value = Value(1.0);
    p.keyframes().push_back(a);
    p.keyframes().push_back(b);

    // Hold mantem o valor ate a proxima key. E o que se usa para ligar e
    // desligar algo sem rampa.
    CHECK_NEAR(p.evaluate(0.99).asDouble(), 0.0, 1e-9);
    CHECK_NEAR(p.evaluate(1.0).asDouble(), 1.0, 1e-9);
}

LUMINA_TEST(Property, insercaoEOrdenacao) {
    Property p(Value(0.0), PropertyUi::Slider);
    p.setKeyframe(1.0, Value(10.0));
    p.setKeyframe(0.0, Value(0.0));
    p.setKeyframe(0.5, Value(5.0));

    // As keys ficam ordenadas por tempo, independentemente da ordem de
    // insercao - e o que a timeline e o editor de curvas assumem.
    CHECK(p.keyframes().size() == 3);
    CHECK(p.keyframes()[0].time == 0.0);
    CHECK(p.keyframes()[1].time == 0.5);
    CHECK(p.keyframes()[2].time == 1.0);

    // Inserir no mesmo tempo substitui, nao duplica.
    p.setKeyframe(0.5, Value(7.0));
    CHECK(p.keyframes().size() == 3);
    CHECK_NEAR(p.evaluate(0.5).asDouble(), 7.0, 1e-9);
}

LUMINA_TEST(Property, coercaoDeTipo) {
    // Ao carregar um arquivo onde a propriedade mudou de Double para Vec2, a
    // Property tem de converter e nao recusar o valor.
    Property p(Value(0.0), PropertyUi::Position);
    p.setForceType(ValueType::Vec2);
    p.setBaseValue(Value(3.0));
    CHECK(p.type() == ValueType::Vec2);
    CHECK_NEAR(p.baseValue().asVec2().x, 3.0, 1e-9);
}

LUMINA_TEST(Transform, composicao) {
    Transform t = Transform::translation(Vec2{10, 20}) *
                  Transform::scale(Vec2{2, 2}) *
                  Transform::rotationZ(0.0);

    const Vec2 p = t.applyPoint(Vec2{1, 1});
    CHECK_NEAR(p.x, 12.0, 1e-9);
    CHECK_NEAR(p.y, 22.0, 1e-9);

    // Inverter e desfazer.
    Transform inv;
    CHECK(t.invert(inv));
    const Vec2 back = inv.applyPoint(p);
    CHECK_NEAR(back.x, 1.0, 1e-9);
    CHECK_NEAR(back.y, 1.0, 1e-9);
}

LUMINA_TEST(Transform, escalaZeroEDegenerada) {
    Transform t = Transform::scale(Vec2{0, 0});
    // Escala zero nao tem inversa: a funcao precisa falhar em vez de dividir
    // por zero e gerar NaN no shader.
    Transform inv;
    CHECK(!t.invert(inv));
}

LUMINA_TEST(Node, propriedadesPadrao) {
    Node node(1, "test.node", "Teste");
    node.setTransformProperties();

    CHECK(node.findProperty("position") != nullptr);
    CHECK(node.findProperty("opacity") != nullptr);

    // Escala comeca em 100% (1:1), nao em 1.0 pixel.
    const Transform t = node.transformAt(0.0);
    const Vec2 p = t.applyPoint(Vec2{100, 100});
    CHECK_NEAR(p.x, 100.0, 1e-6);
    CHECK_NEAR(p.y, 100.0, 1e-6);
}

LUMINA_TEST(Node, transformAninhado) {
    Node node(1, "test.node", "Teste");
    node.setTransformProperties();

    if (Property* p = node.findProperty("position")) {
        p->setBaseValue(Value(Vec2{50, 25}));
    }
    if (Property* p = node.findProperty("scale")) {
        p->setBaseValue(Value(Vec2{200, 200}));
    }

    const Transform t = node.transformAt(0.0);
    // Escala 200% do ponto (100,100) = (200,200), mais posicao (50,25).
    const Vec2 p = t.applyPoint(Vec2{100, 100});
    CHECK_NEAR(p.x, 250.0, 1e-6);
    CHECK_NEAR(p.y, 225.0, 1e-6);
}

LUMINA_TEST(NodeGraph, conexaoSimples) {
    NodeGraph graph;
    Node* a = graph.createNode("src.solid", "A");
    Node* b = graph.createNode("fx.blur", "B");
    CHECK(a != nullptr);
    CHECK(b != nullptr);
    if (!a || !b) return;

    CHECK(graph.connect(a->id(), 0, b->id(), 0));
    CHECK(b->isConnected(0));
    CHECK_EQ(graph.connections().size(), size_t(1));
    CHECK(graph.isAcyclic());
}

LUMINA_TEST(NodeGraph, recusaCiclo) {
    NodeGraph graph;
    Node* a = graph.createNode("src.solid", "A");
    Node* b = graph.createNode("fx.blur", "B");
    Node* c = graph.createNode("cc.brightnessContrast", "C");
    if (!a || !b || !c) return;

    CHECK(graph.connect(a->id(), 0, b->id(), 0));
    CHECK(graph.connect(b->id(), 0, c->id(), 0));

    // a -> b -> c ja existe. Tentar ligar c de volta a a cria um ciclo e tem
    // de ser recusado, senao a avaliacao entra em recursao infinita.
    CHECK(!graph.connect(c->id(), 0, a->id(), 0));
    CHECK(!graph.connect(b->id(), 0, a->id(), 0));
    CHECK(graph.isAcyclic());
}

LUMINA_TEST(NodeGraph, ordemTopologica) {
    NodeGraph graph;
    Node* a = graph.createNode("src.solid", "A");
    Node* b = graph.createNode("fx.blur", "B");
    Node* c = graph.createNode("merge.over", "C");
    if (!a || !b || !c) return;

    // Conecta na ordem inversa de criacao, para provar que a ordem importa e
    // nao apenas a ordem de insercao.
    graph.connect(b->id(), 0, c->id(), 0);
    graph.connect(a->id(), 0, b->id(), 0);

    const auto order = graph.topologicalOrder();
    CHECK(order.size() == 3);

    const auto position = [&order](NodeId id) {
        return std::find(order.begin(), order.end(), id) - order.begin();
    };
    CHECK(position(a->id()) < position(b->id()));
    CHECK(position(b->id()) < position(c->id()));
}

LUMINA_TEST(NodeGraph, remocaoDesconecta) {
    NodeGraph graph;
    Node* a = graph.createNode("src.solid", "A");
    Node* b = graph.createNode("fx.blur", "B");
    if (!a || !b) return;

    graph.connect(a->id(), 0, b->id(), 0);
    CHECK(b->isConnected(0));

    CHECK(graph.removeNode(a->id()));
    // Remover a origem tem de soltar a entrada que apontava para ela, senao o
    // grafo fica com uma conexao orfa que quebra a renderizacao.
    CHECK(!b->isConnected(0));
    CHECK(graph.isAcyclic());
}

LUMINA_TEST(NodeGraph, ancestraisEDescendentes) {
    NodeGraph graph;
    Node* a = graph.createNode("src.solid", "A");
    Node* b = graph.createNode("fx.blur", "B");
    Node* c = graph.createNode("cc.brightnessContrast", "C");
    if (!a || !b || !c) return;

    graph.connect(a->id(), 0, b->id(), 0);
    graph.connect(b->id(), 0, c->id(), 0);

    const auto anc = graph.ancestors(c->id());
    CHECK(std::find(anc.begin(), anc.end(), a->id()) != anc.end());
    CHECK(std::find(anc.begin(), anc.end(), b->id()) != anc.end());

    const auto desc = graph.descendants(a->id());
    CHECK(std::find(desc.begin(), desc.end(), c->id()) != desc.end());
}

LUMINA_TEST(Composition, conversaoDeTempo) {
    Composition comp(1, "Teste", Size{1920, 1080}, 30.0, 10.0);
    CHECK(comp.timeToFrame(0.0) == 0);
    CHECK(comp.timeToFrame(1.0) == 30);
    CHECK_NEAR(comp.frameToTime(30), 1.0, 1e-9);
    CHECK(comp.durationInFrames() == 300);
}

LUMINA_TEST(Composition, areaDeTrabalhoLimitada) {
    Composition comp(1, "Teste", Size{1920, 1080}, 30.0, 5.0);
    comp.setWorkArea(TimeRange{-2.0, 100.0});
    // A area de trabalho nunca pode passar da duracao.
    CHECK(comp.workArea().in >= 0.0);
    CHECK(comp.workArea().out <= 5.0);
}

LUMINA_TEST(Blending, nomesEstaveis) {
    // O nome do modo de mesclagem vai para o arquivo e para o shader: se
    // mudar, projetos antigos param de abrir.
    CHECK_EQ(std::string(blendModeName(BlendMode::Over)), std::string("Over"));
    CHECK(blendModeFromName("Screen") == BlendMode::Screen);
    CHECK(blendModeFromName("Multiply") == BlendMode::Multiply);
    CHECK(blendModeFromName("nao existe") == BlendMode::Over);
    CHECK(blendModeUsesBackdrop(BlendMode::Screen));
    CHECK(blendModeIsSeparable(BlendMode::Screen));
    CHECK(!blendModeIsSeparable(BlendMode::Color_));
}

LUMINA_TEST(Timeline, insercaoOrdenada) {
    Timeline tl;
    Stack* v1 = tl.addStack("V1");
    CHECK(v1 != nullptr);
    if (!v1) return;

    Clip a;
    a.id = 1;
    a.start = 2.0;
    a.sourceIn = 0.0;
    a.sourceOut = 1.0;

    Clip b = a;
    b.id = 2;
    b.start = 0.0;

    v1->insert(a);
    v1->insert(b);

    // Inserir em ordem inversa precisa manter o clip ordenado.
    CHECK(v1->clips().size() == 2);
    CHECK(v1->clips()[0].id == 2);
    CHECK(v1->clips()[1].id == 1);
}

LUMINA_TEST(Timeline, duracaoComVelocidade) {
    Clip clip;
    clip.sourceIn = 0.0;
    clip.sourceOut = 4.0;
    clip.speed = 0.5;
    // 4 segundos de origem a meia velocidade ocupam 8 na timeline.
    CHECK_NEAR(clip.duration(), 8.0, 1e-9);
    CHECK_NEAR(clip.end(), clip.start + 8.0, 1e-9);

    // Tempo da timeline para tempo de origem.
    CHECK_NEAR(clip.toSourceTime(clip.start), 0.0, 1e-9);
    CHECK_NEAR(clip.toSourceTime(clip.start + 2.0), 1.0, 1e-9);
}

LUMINA_TEST(Project, composicaoPadrao) {
    Project project;
    project.createDefaultComposition();
    CHECK(project.compositionCount() == 1);

    const CompId id = project.compositions().begin()->first;
    Composition* comp = project.composition(id);
    CHECK(comp != nullptr);
    if (!comp) return;

    // O grafo de boas-vindas tem de produzir pixels: um no Solido ligado a um
    // Merge, com saida definida.
    CHECK(!comp->graph().isEmpty());
    CHECK(comp->graph().output() != kInvalidId);
    CHECK(comp->graph().isAcyclic());
}

LUMINA_TEST(Project, duplicacaoIsolaGrafo) {
    Project project;
    const CompId first = project.createComposition("A", Size{1920, 1080}, 30.0, 5.0);
    Composition* comp = project.composition(first);
    if (!comp) return;

    Node* solid = comp->graph().createNode("src.solid", "Fundo");
    Node* blur = comp->graph().createNode("fx.blur", "Desfoque");
    if (solid && blur) {
        comp->graph().connect(solid->id(), 0, blur->id(), 0);
        comp->graph().setOutput(blur->id());
    }

    const CompId copy = project.duplicateComposition(first, "A copia");
    Composition* duplicated = project.composition(copy);
    CHECK(duplicated != nullptr);
    if (!duplicated) return;

    CHECK(duplicated->graph().size() == 2);
    CHECK(duplicated->graph().output() != kInvalidId);
    CHECK(duplicated->graph().isAcyclic());
    CHECK_EQ(duplicated->name(), std::string("A copia"));
}

LUMINA_TEST(Project, naoRemoveUltimaComposicao) {
    Project project;
    const CompId only = project.createComposition("Unica", Size{1280, 720}, 30.0, 5.0);
    // Um projeto sem nenhuma composicao nao tem area de trabalho para
    // renderizar, entao a ultima nunca e removida.
    CHECK(!project.removeComposition(only));
    CHECK(project.compositionCount() == 1);
}

LUMINA_TEST(Project, removeComposicaoLimpaClips) {
    Project project;
    const CompId a = project.createComposition("A", Size{1920, 1080}, 30.0, 5.0);
    const CompId b = project.createComposition("B", Size{1920, 1080}, 30.0, 5.0);

    Stack* v1 = project.timeline().addStack("V1");
    if (!v1) return;
    Clip clip;
    clip.id = 1;
    clip.comp = b;
    clip.start = 0.0;
    clip.sourceIn = 0.0;
    clip.sourceOut = 2.0;
    v1->insert(clip);

    CHECK(project.removeComposition(b));
    // O clip apontava para a composicao removida e tem de sumir junto, senao
    // a timeline fica com um buraco que nao renderiza nada.
    CHECK(v1->clips().empty());
    CHECK(project.composition(a) != nullptr);
}

LUMINA_TEST(MediaPool, proxyEOrcamento) {
    MediaPool pool;
    MediaItem item;
    item.type = MediaType::Video;
    item.path = "/tmp/video.mp4";
    item.size = Size{3840, 2160};
    const MediaId id = pool.add(item);
    CHECK(id != 0);
    CHECK_EQ(pool.size(), size_t(1));

    // 4K precisa de proxy; 720p nao.
    CHECK(pool.needsProxy(1920).size() == 1);

    if (MediaItem* stored = pool.find(id)) {
        stored->proxyPath = "/tmp/proxy.mov";
        stored->proxyScale = 0.25;
    }
    CHECK(pool.needsProxy(1920).empty());
    CHECK_EQ(pool.proxyFileCount(), size_t(1));
    CHECK(shouldUseProxyForTest(*pool.find(id), true));
}

LUMINA_TEST(NodeRegistry, tiposRegistrados) {
    // O registro precisa ter os tipos basicos de um grafo novo.
    NodeRegistry::instance().registerType(makeSolidDescForTest());
    CHECK(NodeRegistry::instance().has("src.solid"));
    CHECK(NodeRegistry::instance().has("merge.over"));
    CHECK(NodeRegistry::instance().has("fx.blur"));

    const NodeTypeDesc* desc = NodeRegistry::instance().desc("fx.blur");
    CHECK(desc != nullptr);
    if (!desc) return;
    CHECK_EQ(desc->displayName, std::string("Desfoque"));
    CHECK_EQ(desc->inputCount(), 1);
    CHECK(desc->isFilter);
    CHECK(!desc->isGenerator);
}
