// SourceNodes.cpp - Nos geradores: nada entra, algo sai.
//
// Um gerador e a base de quase todo grafo, porque permite construir uma cena
// sem nenhum arquivo externo - o que abre o programa instantaneamente e
// mantem o projeto leve.
#include "NodeHelpers.h"

namespace lmn::nodes {
namespace {

using gpu::ShaderProgram;
using gpu::setPropertyBool;
using gpu::setPropertyColor;
using gpu::setPropertyF;
using gpu::setPropertyI;
using gpu::setPropertyV2;
using gpu::setPropertyV4;

// ---------------------------------------------------------------------------
// src.solid
// ---------------------------------------------------------------------------

void registerSolid() {
    SpecBuilder b;
    b.color("u_colorA", Color{0.5, 0.5, 0.5, 1.0}, "Cor", "Cor em espaco linear")
        .color("u_colorB", Color{0.0, 0.0, 0.0, 1.0}, "Cor", "Cor final do gradiente")
        .choice("u_gradient", "solido",
                {kGradientTypes, kGradientTypes + kGradientTypeCount}, "Gradiente",
                "Solido ou um dos cinco gradientes")
        .vec2("u_start", 0.0, 0.0, "Gradiente", "Inicio, em UV")
        .vec2("u_end", 1.0, 1.0, "Gradiente", "Fim, em UV")
        .f("u_repeat", 0.0, 0.0, 64.0, "Gradiente", "Repeticoes ao longo da UV")
        .f("u_dither", 1.0, 0.0, 4.0, "Qualidade",
           "Quebra banding em cores suaves. 0 desliga.");

    registerNode(
        makeDesc("src.solid", "Solido", "Fonte", "shaders/solid.frag", b.props, {},
                 "Preenche a composicao com uma cor ou gradiente. E o primeiro no "
                 "de qualquer grafo novo."),
        [](ShaderProgram& p, const Node& n, Time t, const ExpressionContext& ctx) {
            setPropertyColor(p, n, "u_colorA", t, ctx);
            setPropertyColor(p, n, "u_colorB", t, ctx);
            setPropertyV2(p, n, "u_start", t, ctx);
            setPropertyV2(p, n, "u_end", t, ctx);
            setPropertyF(p, n, "u_repeat", t, ctx);
            setPropertyF(p, n, "u_dither", t, ctx);
            gpu::setPropertyEnum(p, n, "u_gradient", t, ctx, kGradientTypes,
                                 kGradientTypeCount, 0);
        });
}

// ---------------------------------------------------------------------------
// src.noise
// ---------------------------------------------------------------------------

void registerNoise() {
    SpecBuilder b;
    b.choice("u_type", "fbm suave", {kNoiseTypes, kNoiseTypes + kNoiseTypeCount},
             "Tipo", "Algoritmo do ruido")
        .color("u_colorA", Color{0.0, 0.0, 0.0, 1.0}, "Cor")
        .color("u_colorB", Color{1.0, 1.0, 1.0, 1.0}, "Cor")
        .f("u_scale", 4.0, 0.01, 512.0, "Escala", "Repeticoes da textura")
        .f("u_contrast", 0.0, 0.0, 1.0, "Forma", "Endurece as transicoes")
        .f("u_warp", 0.0, 0.0, 4.0, "Forma", "Deforma o ruido com ele mesmo")
        .f("u_animate", 0.0, 0.0, 1.0, "Tempo", "0 = estatico, 1 = evolui")
        .check("u_tiled", false, "Forma", "Repete em vez de esticar")
        .vec4("u_transform", Vec4{0.5, 0.5, 1.0, 1.0}, "Posicao");

    registerNode(
        makeDesc("src.noise", "Ruido", "Fonte", "shaders/noise_texture.frag", b.props,
                 {}, "Textura procedural: fumaca, turbulencia, chamas, listas. "
                     "Nao usa imagem e nao pesa no projeto."),
        [](ShaderProgram& p, const Node& n, Time t, const ExpressionContext& ctx) {
            setPropertyColor(p, n, "u_colorA", t, ctx);
            setPropertyColor(p, n, "u_colorB", t, ctx);
            setPropertyF(p, n, "u_scale", t, ctx);
            setPropertyF(p, n, "u_contrast", t, ctx);
            setPropertyF(p, n, "u_warp", t, ctx);
            setPropertyF(p, n, "u_animate", t, ctx);
            setPropertyBool(p, n, "u_tiled", t, ctx);
            setPropertyV4(p, n, "u_transform", t, ctx);
            gpu::setPropertyEnum(p, n, "u_type", t, ctx, kNoiseTypes, kNoiseTypeCount, 0);
        });
}

// ---------------------------------------------------------------------------
// src.shape
// ---------------------------------------------------------------------------

void registerShape() {
    SpecBuilder b;
    b.choice("u_shape", "retangulo", {kShapeTypes, kShapeTypes + kShapeTypeCount},
             "Forma")
        .color("u_fillColor", Color{1.0, 1.0, 1.0, 1.0}, "Preenchimento")
        .color("u_strokeColor", Color{0.0, 0.0, 0.0, 1.0}, "Contorno")
        .f("u_strokeWidth", 0.0, 0.0, 200.0, "Contorno", "Espessura em pixels")
        .f("u_feather", 0.0, 0.0, 200.0, "Borda", "Suavidade, em pixels")
        .f("u_roundness", 0.0, 0.0, 1.0, "Forma", "0 = canto reto, 1 = pill")
        .i("u_sides", 6, 3, 16, "Forma", "Lados do poligono")
        .f("u_innerRadius", 0.4, 0.05, 0.95, "Forma", "Raio interno da estrela")
        .vec2("u_center", 0.5, 0.5, "Posicao", "Centro em UV")
        .vec2("u_halfSize", 0.25, 0.25, "Tamanho", "Meia-extensao em UV")
        .angle("u_rotation", 0.0, "Posicao")
        .check("u_fillEnabled", true, "Preenchimento")
        .check("u_strokeEnabled", false, "Contorno");

    registerNode(
        makeDesc("src.shape", "Forma", "Fonte", "shaders/shape.frag", b.props, {},
                 "Retangulo, elipse, triangulo, estrela e poligono com "
                 "preenchimento, contorno e borda suave. Serve tambem como "
                 "mascara."),
        [](ShaderProgram& p, const Node& n, Time t, const ExpressionContext& ctx) {
            setPropertyColor(p, n, "u_fillColor", t, ctx);
            setPropertyColor(p, n, "u_strokeColor", t, ctx);
            setPropertyF(p, n, "u_strokeWidth", t, ctx);
            setPropertyF(p, n, "u_feather", t, ctx);
            setPropertyF(p, n, "u_roundness", t, ctx);
            setPropertyF(p, n, "u_sides", t, ctx);
            setPropertyF(p, n, "u_innerRadius", t, ctx);
            setPropertyV2(p, n, "u_center", t, ctx);
            setPropertyV2(p, n, "u_halfSize", t, ctx);
            setPropertyF(p, n, "u_rotation", t, ctx);
            setPropertyBool(p, n, "u_fillEnabled", t, ctx);
            setPropertyBool(p, n, "u_strokeEnabled", t, ctx);
            gpu::setPropertyEnum(p, n, "u_shape", t, ctx, kShapeTypes, kShapeTypeCount, 0);
        });
}

// ---------------------------------------------------------------------------
// src.checkerboard - fundo do visualizador, nunca exportado
// ---------------------------------------------------------------------------

void registerCheckerboard() {
    SpecBuilder b;
    b.color("u_colorA", Color{0.16, 0.16, 0.16, 1.0}, "Cores")
        .color("u_colorB", Color{0.22, 0.22, 0.22, 1.0}, "Cores")
        .i("u_squares", 12, 4, 64, "Cores", "Tamanho do quadrado em pixels");

    registerNode(
        makeDesc("src.checkerboard", "Xadrez", "Fonte", "shaders/checkerboard.frag",
                 b.props, {}, "Fundo xadrez para ver o que e transparente. "
                              "Fica no grafo, mas e ignorado na exportacao."),
        [](ShaderProgram& p, const Node& n, Time t, const ExpressionContext& ctx) {
            setPropertyColor(p, n, "u_colorA", t, ctx);
            setPropertyColor(p, n, "u_colorB", t, ctx);
            setPropertyF(p, n, "u_squares", t, ctx);
        });
}

}  // namespace

void registerSourceNodes() {
    registerSolid();
    registerNoise();
    registerShape();
    registerCheckerboard();
}

}  // namespace lmn::nodes
