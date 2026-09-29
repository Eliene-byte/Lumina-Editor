// MergeNodes.cpp - Nos que combinam entradas: Merge e ajuste de alpha.
#include "NodeHelpers.h"

namespace lmn::nodes {
namespace {

using gpu::ShaderProgram;
using gpu::setPropertyBool;
using gpu::setPropertyColor;
using gpu::setPropertyF;
using gpu::setPropertyI;

void registerMerge() {
    SpecBuilder b;
    b.f("u_applyOpacity", 1.0, 0.0, 1.0, "Mesclagem",
         "0 = Apply Opacity do After Effects (opacidade age no resultado)")
        .color("u_clearColor", Color{0.0, 0.0, 0.0, 1.0}, "Interno",
               "Cor usada quando a entrada esta desconectada");

    // Merge nao tem transformacao propria: quem a tem e a entrada da frente.
    NodeTypeDesc d = makeDesc("merge.over", "Merge", "Mesclagem", "shaders/merge.frag",
                              b.props, {"Atras", "Frente"},
                              "Combina duas entradas com o modo de mesclagem do "
                              "no. A entrada 1 e o fundo, a entrada 2 e a frente.");
    d.hasTransform = false;
    d.isMerger = true;
    registerNode(d, [](ShaderProgram& p, const Node& n, Time t,
                       const ExpressionContext& ctx) {
        setPropertyF(p, n, "u_applyOpacity", t, ctx);
        setPropertyColor(p, n, "u_clearColor", t, ctx);
        // O modo vem do campo blendMode do no, nao de uma Property: e um
        // estado estrutural, e nao um valor animavel.
        p.setInt("u_mode", static_cast<int>(n.blendMode()));
    });
}

void registerSetAlpha() {
    SpecBuilder b;
    b.f("u_alpha", 1.0, 0.0, 1.0, "Alpha")
        .check("u_inheritAlpha", false, "Alpha", "Multiplica pelo alpha da entrada")
        .check("u_outputStraight", false, "Saida", "Devolve a cor sem multiplicar por a")
        .check("u_useClearColor", false, "Saida", "Substitui a cor, nao so o alpha")
        .color("u_clearColor", Color{0.0, 0.0, 0.0, 1.0}, "Saida");

    NodeTypeDesc d = makeDesc("set.alpha", "Definir Alpha", "Mesclagem",
                              "shaders/set_alpha.frag", b.props, {"Entrada"},
                              "Fixa o canal alpha do resultado.");
    d.hasTransform = false;
    registerNode(d, [](ShaderProgram& p, const Node& n, Time t,
                       const ExpressionContext& ctx) {
        setPropertyF(p, n, "u_alpha", t, ctx);
        setPropertyBool(p, n, "u_inheritAlpha", t, ctx);
        setPropertyBool(p, n, "u_outputStraight", t, ctx);
        setPropertyBool(p, n, "u_useClearColor", t, ctx);
        setPropertyColor(p, n, "u_clearColor", t, ctx);
    });
}

void registerPassThrough() {
    // Nao tem shader proprio: o Compositor detecta a flag e devolve a entrada.
    // Existe como tipo para o usuario ter uma forma explicita de "nao mexer
    // neste ponto" e um lugar para pendurar efeitos sem alterar a imagem.
    NodeTypeDesc d;
    d.type = "util.passThrough";
    d.displayName = "Repassar";
    d.category = "Utilitario";
    d.shader = "shaders/copy.frag";
    d.inputNames = {"Entrada"};
    d.hasTransform = false;
    d.isFilter = true;
    d.description = "Devolve a entrada sem alterar nada. Serve como ponto de "
                    "ramo e para deixar um efeito em espera.";

    registerNode(d, [](ShaderProgram& p, const Node&, Time, const ExpressionContext&) {
        p.setInt("u_straighten", 0);
        p.setInt("u_premultiply", 0);
        p.setInt("u_useTransform", 0);
    });
}

}  // namespace

void registerMergeNodes() {
    registerMerge();
    registerSetAlpha();
    registerPassThrough();
}

}  // namespace lmn::nodes
