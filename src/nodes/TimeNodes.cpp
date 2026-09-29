// TimeNodes.cpp - Nos de tempo: reamostragem e transformacao de velocidade.
#include "NodeHelpers.h"

namespace lmn::nodes {
namespace {

using gpu::ShaderProgram;
using gpu::setPropertyF;
using gpu::setPropertyV2;

void registerTimeStretch() {
    SpecBuilder b;
    b.choice("u_mode", "linear", {kTimeStretchModes, kTimeStretchModes + kTimeStretchModeCount},
             "Modo", "Segurar, misturar linearmente ou borrar entre quadros")
        .f("u_mix", 0.0, 0.0, 1.0, "Tempo", "0 = quadro base, 1 = seguinte")
        .f("u_denoise", 0.5, 0.0, 1.0, "Qualidade",
           "Devolve parte do micro-contraste perdido na mistura")
        .vec2("u_frameA", 0.5, 0.5, "Interno")
        .vec2("u_frameB", 0.5, 0.5, "Interno");

    NodeTypeDesc d = makeDesc("retime.timeStretch", "Reamostrar tempo", "Tempo",
                              "shaders/time_stretch.frag", b.props, {"Entrada"},
                              "Controla a velocidade. Em velocidades baixas, "
                              "evita a escada de quadros duplicados.");
    d.hasTransform = false;
    registerNode(d, [](ShaderProgram& p, const Node& n, Time t,
                       const ExpressionContext& ctx) {
        setPropertyF(p, n, "u_mix", t, ctx);
        setPropertyF(p, n, "u_denoise", t, ctx);
        setPropertyV2(p, n, "u_frameA", t, ctx);
        setPropertyV2(p, n, "u_frameB", t, ctx);
        gpu::setPropertyEnum(p, n, "u_mode", t, ctx, kTimeStretchModes,
                             kTimeStretchModeCount, 1);
    });
}

}  // namespace

void registerTimeNodes() {
    registerTimeStretch();
}

}  // namespace lmn::nodes
