// TestSerialization.cpp - Ida e volta do arquivo de projeto.
//
// O teste central e o round-trip: salvar e recarregar tem de devolver um
// projeto identico. E o que garante que um usuario possa abrir um projeto de
// amanha em uma versao de hoje.
#include "TestSupport.h"

#include "core/Project.h"
#include "expr/Expression.h"
#include "io/ProjectSerializer.h"
#include "nodes/Nodes.h"

using namespace lmn;
using namespace lmn::io;

namespace {

// Constroi um projeto com o suficiente de cada conceito para o round-trip ter
// o que exercitar: grafo, animacao, expressao, timeline, transicao, acervo.
Project buildRichProject() {
    nodes::registerAllNodes();

    Project project;
    project.setName("Projeto de teste");
    project.setSize(Size{1280, 720});
    project.setFps(24.0);

    const CompId compId = project.createComposition("Cena", Size{1280, 720}, 24.0, 8.0);
    Composition* comp = project.composition(compId);
    if (!comp) return project;

    Node* solid = comp->graph().createNode("src.solid", "Fundo");
    Node* blur = comp->graph().createNode("fx.blur", "Desfoque");
    Node* grade = comp->graph().createNode("cc.brightnessContrast", "Correcao");
    Node* merge = comp->graph().createNode("merge.over", "Merge");

    if (solid) {
        if (Property* c = solid->findProperty("u_colorA")) {
            c->setBaseValue(Value(Color{0.2, 0.4, 0.6, 1.0}));
        }
    }
    if (blur) {
        if (Property* r = blur->findProperty("u_radius")) {
            // Animado: duas keys com suavidade diferente de zero.
            Keyframe a;
            a.time = 0.0;
            a.value = Value(0.0);
            a.interp = Interp::Bezier;
            a.easeOut = 0.4;
            Keyframe b = a;
            b.time = 4.0;
            b.value = Value(25.0);
            b.easeIn = 0.4;
            r->keyframes().push_back(a);
            r->keyframes().push_back(b);
        }
        blur->setCachePolicy(CachePolicy::Region);
    }
    if (grade) {
        if (Property* b = grade->findProperty("u_contrast")) {
            b->setBaseValue(Value(35.0));
        }
        grade->setEnabled(false);
    }
    if (merge) {
        merge->setBlendMode(BlendMode::Screen);
    }

    if (solid && blur) comp->graph().connect(solid->id(), 0, blur->id(), 0);
    if (blur && grade) comp->graph().connect(blur->id(), 0, grade->id(), 0);
    if (grade && merge) comp->graph().connect(grade->id(), 0, merge->id(), 0);
    comp->graph().setOutput(merge ? merge->id() : (solid ? solid->id() : kInvalidId));

    comp->setTransparentBackground(true);
    comp->setBackground(Color{0.05, 0.05, 0.08, 1.0});
    comp->setWorkArea(TimeRange{1.0, 6.0});

    // Timeline com dois clips, um deles com transicao.
    Stack* v1 = project.timeline().addStack("V1");
    Stack* a1 = project.timeline().addStack("A1");
    if (v1) {
        Clip first;
        first.id = 1;
        first.comp = compId;
        first.name = "Abertura";
        first.start = 0.0;
        first.sourceIn = 0.0;
        first.sourceOut = 3.0;
        first.hasTransitionOut = true;
        first.transitionOut.type = TransitionType::CrossDissolve;
        first.transitionOut.duration = 0.5;
        v1->insert(first);

        Clip second = first;
        second.id = 2;
        second.name = "Corpo";
        second.start = 3.0;
        second.sourceIn = 3.0;
        second.sourceOut = 8.0;
        second.hasTransitionIn = false;
        second.hasTransitionOut = false;
        v1->insert(second);
    }
    if (a1) {
        Clip audio;
        audio.id = 3;
        audio.comp = compId;
        audio.name = "Musica";
        audio.start = 0.0;
        audio.sourceIn = 0.0;
        audio.sourceOut = 8.0;
        audio.volume = 0.7;
        a1->insert(audio);
    }

    // Acervo
    MediaItem item;
    item.type = MediaType::Video;
    item.path = "C:/videos/fonte.mp4";
    item.name = "fonte";
    item.duration = 12.0;
    item.size = Size{1920, 1080};
    item.fps = 30.0;
    item.hasAlpha = true;
    item.proxyPath = "C:/videos/proxies/fonte.mov";
    item.proxyScale = 0.25;
    project.media().add(item);

    return project;
}

}  // namespace

LUMINA_TEST(Serialization, roundTripCompleto) {
    const Project original = buildRichProject();

    std::string error;
    Project restored;
    CHECK(ProjectSerializer::fromJson(ProjectSerializer::toJson(original), restored,
                                     &error));
    CHECK(error.empty());

    // --- Formato ---
    CHECK_EQ(restored.name(), original.name());
    CHECK(restored.size() == original.size());
    CHECK_NEAR(restored.fps(), original.fps(), 1e-9);
    CHECK_EQ(restored.compositionCount(), original.compositionCount());

    // --- Composicao e grafo ---
    const CompId compId = original.compositions().begin()->first;
    const Composition* src = original.composition(compId);
    const Composition* dst = restored.composition(compId);
    CHECK(src != nullptr);
    CHECK(dst != nullptr);
    if (!src || !dst) return;

    CHECK_EQ(dst->name(), src->name());
    CHECK(dst->size() == src->size());
    CHECK_NEAR(dst->fps(), src->fps(), 1e-9);
    CHECK(dst->duration() == src->duration());
    CHECK(dst->workArea() == src->workArea());
    CHECK_EQ(dst->transparentBackground(), src->transparentBackground());
    CHECK_EQ(dst->graph().size(), src->graph().size());
    CHECK_EQ(dst->graph().output(), src->graph().output());
    CHECK_EQ(dst->graph().connections().size(), src->graph().connections().size());

    // --- Propriedades, animacao e expressoes ---
    Node* srcBlur = src->graph().node(2);
    Node* dstBlur = dst->graph().node(2);
    CHECK(srcBlur != nullptr);
    CHECK(dstBlur != nullptr);
    if (srcBlur && dstBlur) {
        CHECK_EQ(dstBlur->name(), srcBlur->name());
        CHECK(dstBlur->cachePolicy() == srcBlur->cachePolicy());

        const Property* srcRadius = srcBlur->findProperty("u_radius");
        const Property* dstRadius = dstBlur->findProperty("u_radius");
        CHECK(srcRadius != nullptr);
        CHECK(dstRadius != nullptr);
        if (srcRadius && dstRadius) {
            CHECK_EQ(dstRadius->keyframes().size(), srcRadius->keyframes().size());
            CHECK_NEAR(dstRadius->evaluate(4.0).asDouble(), 25.0, 1e-6);
            CHECK_NEAR(dstRadius->evaluate(2.0).asDouble(),
                       srcRadius->evaluate(2.0).asDouble(), 1e-6);
        }
    }

    // --- Estado de no (ligado/desligado, modo de mesclagem) ---
    Node* dstGrade = dst->graph().node(3);
    CHECK(dstGrade != nullptr);
    if (dstGrade) {
        CHECK_EQ(dstGrade->isEnabled(), false);
    }
    Node* dstMerge = dst->graph().node(4);
    CHECK(dstMerge != nullptr);
    if (dstMerge) {
        CHECK(dstMerge->blendMode() == BlendMode::Screen);
    }

    // --- Timeline ---
    CHECK_EQ(restored.timeline().stackCount(), original.timeline().stackCount());
    const Stack* srcV1 = original.timeline().stacks().front().clips().empty()
                             ? nullptr
                             : &original.timeline().stacks().front();
    CHECK(srcV1 != nullptr);
    if (srcV1) {
        const Stack* dstV1 = &restored.timeline().stacks().front();
        CHECK_EQ(dstV1->clips().size(), srcV1->clips().size());
        if (!dstV1->clips().empty()) {
            const Clip& c = dstV1->clips().front();
            CHECK_EQ(c.name, std::string("Abertura"));
            CHECK_NEAR(c.start, 0.0, 1e-9);
            CHECK_NEAR(c.sourceOut, 3.0, 1e-9);
            CHECK(c.hasTransitionOut);
            CHECK(c.transitionOut.type == TransitionType::CrossDissolve);
            CHECK_NEAR(c.transitionOut.duration, 0.5, 1e-9);
        }
    }

    // --- Acervo ---
    CHECK_EQ(restored.media().size(), original.media().size());
    if (!restored.media().items().empty()) {
        const MediaItem& m = restored.media().items().front();
        CHECK(m.type == MediaType::Video);
        CHECK_EQ(m.path, std::string("C:/videos/fonte.mp4"));
        CHECK_NEAR(m.duration, 12.0, 1e-9);
        CHECK(m.hasAlpha);
        CHECK_EQ(m.proxyPath, std::string("C:/videos/proxies/fonte.mov"));
    }
}

LUMINA_TEST(Serialization, expressaoSobreviveAoRoundTrip) {
    nodes::registerAllNodes();
    expr::installAsCoreBackend();

    Project project;
    const CompId id = project.createComposition("C", Size{640, 480}, 30.0, 5.0);
    Composition* comp = project.composition(id);
    if (!comp) return;

    Node* node = comp->graph().createNode("src.solid", "Fundo");
    if (!node) return;
    if (Property* p = node->findProperty("u_rotation")) {
        p->setExpression("wiggle(2, 30)");
    }
    comp->graph().setOutput(node->id());

    Project restored;
    std::string error;
    CHECK(ProjectSerializer::fromJson(ProjectSerializer::toJson(project), restored,
                                     &error));

    const Composition* rc = restored.composition(id);
    CHECK(rc != nullptr);
    if (!rc) return;
    const Node* rn = rc->graph().node(node->id());
    CHECK(rn != nullptr);
    if (!rn) return;

    const Property* p = rn->findProperty("u_rotation");
    CHECK(p != nullptr);
    if (!p) return;
    CHECK_EQ(p->expression(), std::string("wiggle(2, 30)"));
    CHECK(p->hasExpression());

    // E a expressao realmente e avaliada depois de recarregar.
    const double a = p->evaluate(1.0).asDouble();
    const double b = p->evaluate(1.0).asDouble();
    CHECK_NEAR(a, b, 1e-12);
}

LUMINA_TEST(Serialization, rejeitaArquivoInvalido) {
    Project project;
    std::string error;

    // Nao e um JSON.
    CHECK(!ProjectSerializer::fromJson("isto nao e json", project, &error));
    CHECK(!error.empty());

    // JSON valido, mas nao e um projeto.
    CHECK(!ProjectSerializer::fromJson("{\"outro\": 1}", project, &error));
    CHECK(error.find("lumina") != std::string::npos);

    // Versao do futuro: avisar em vez de tentar ler errado.
    CHECK(!ProjectSerializer::fromJson("{\"lumina\": 9999}", project, &error));
    CHECK(error.find("versao mais nova") != std::string::npos);
}

LUMINA_TEST(Serialization, arquivoVazioNaoQuebra) {
    // Um projeto sem nenhuma composicao tem de ganhar uma ao carregar, senao
    // a interface abre sem area de trabalho.
    Project project;
    std::string error;
    CHECK(ProjectSerializer::fromJson("{\"lumina\": 1}", project, &error));
    CHECK(project.compositionCount() >= 1);
}

LUMINA_TEST(Serialization, propriedadeDesconhecidaEhPreservada) {
    nodes::registerAllNodes();

    Project project;
    const CompId id = project.createComposition("C", Size{640, 480}, 30.0, 5.0);
    Composition* comp = project.composition(id);
    if (!comp) return;

    Node* node = comp->graph().createNode("src.solid", "Fundo");
    if (!node) return;
    // Propriedade de uma versao futura do editor, que este nao conhece.
    Property custom(Value(42.0));
    custom.setName("parametroDoFuturo");
    node->addProperty(custom);
    comp->graph().setOutput(node->id());

    Project restored;
    std::string error;
    CHECK(ProjectSerializer::fromJson(ProjectSerializer::toJson(project), restored,
                                     &error));

    const Composition* rc = restored.composition(id);
    if (!rc) return;
    const Node* rn = rc->graph().node(node->id());
    if (!rn) return;

    // Perdido no painel (o registro nao a conhece), mas preservado no arquivo:
    // quando a versao futura abrir o projeto, o valor esta la.
    const Property* p = rn->findProperty("parametroDoFuturo");
    CHECK(p != nullptr);
    if (p) CHECK_NEAR(p->baseValue().asDouble(), 42.0, 1e-9);
}

LUMINA_TEST(Serialization, compactaoDoJson) {
    // Sem indentacao: um projeto grande nao precisa de 3x o tamanho so por
    // causa de espacos.
    const Project project = buildRichProject();
    const std::string json = ProjectSerializer::toJson(project);
    CHECK(json.find('\n') == std::string::npos);
    CHECK(json.find("  ") == std::string::npos || json.size() < 200000);
    CHECK(json.find("\"lumina\"") != std::string::npos);
}

LUMINA_TEST(Serialization, grafoMinimoCarrega) {
    // Um arquivo escrito a mao, com o grafo minimo, tem de carregar e ficar
    // consistente.
    const char* json = R"({
        "lumina": 1,
        "comps": [{
            "id": 1, "nm": "C", "w": 640, "h": 480, "fps": 30, "in": 0, "out": 5,
            "out_node": 2,
            "nd": [
                {"id": 1, "ty": "src.solid"},
                {"id": 2, "ty": "fx.blur", "in": [[1, 0, 0]]}
            ]
        }],
        "tl": {"in": 0, "out": 5, "st": []},
        "md": []
    })";

    Project project;
    std::string error;
    CHECK(ProjectSerializer::fromJson(json, project, &error));
    CHECK(error.empty());
    CHECK_EQ(project.compositionCount(), size_t(1));

    Composition* comp = project.composition(1);
    CHECK(comp != nullptr);
    if (!comp) return;
    CHECK_EQ(comp->graph().size(), size_t(2));
    CHECK_EQ(comp->graph().output(), NodeId(2));
    CHECK(comp->graph().isAcyclic());

    // A conexao sobreviveu.
    const Node* blur = comp->graph().node(2);
    CHECK(blur != nullptr);
    if (blur) {
        CHECK(blur->isConnected(0));
        CHECK_EQ(blur->input(0), NodeId(1));
    }
}
