// FilterNodes.cpp - Filtros: desfoque, brilho, deslocamento, vinheta, matte e
// remocao de fundo.
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

void registerBlur() {
    SpecBuilder b;
    b.f("u_radius", 8.0, 0.0, 500.0, "Desfoque", "Raio em pixels")
        .i("u_quality", 2, 0, 2, "Qualidade", "0 = 5 amostras, 2 = 13")
        .f("u_alphaScale", 1.0, 0.0, 1.0, "Avancado",
           "0 = desfoca so a cor, mantendo o alpha")
        .i("u_wrap", 0, 0, 2, "Avancado", "Limitar, repetir ou espelhar as bordas")
        .fixed("u_directionX", 1.0, "Interno")
        .fixed("u_directionY", 0.0, "Interno");

    registerNode(
        makeDesc("fx.blur", "Desfoque", "Filtro", "shaders/blur.frag", b.props,
                 {"Entrada"},
                 "Desfoque gaussiano em espaco linear, em duas passadas "
                 "separaveis. Roda em qualquer GPU mesmo em PCs modestos."),
        [](ShaderProgram& p, const Node& n, Time t, const ExpressionContext& ctx) {
            setPropertyF(p, n, "u_radius", t, ctx);
            setPropertyI(p, n, "u_quality", t, ctx);
            setPropertyF(p, n, "u_alphaScale", t, ctx);
            setPropertyI(p, n, "u_wrap", t, ctx);
            p.setVec2("u_direction", Vec2{1.0, 0.0});
        });
}

void registerGlow() {
    SpecBuilder b;
    b.f("u_threshold", 0.7, 0.0, 1.0, "Brilho", "So o que passa disso recebe halo")
        .f("u_intensity", 1.0, 0.0, 4.0, "Brilho")
        .f("u_radius", 24.0, 0.0, 400.0, "Brilho", "Alcance em pixels")
        .color("u_tint", Color{1.0, 0.95, 0.85, 1.0}, "Brilho", "Cor do halo")
        .i("u_quality", 0, 0, 1, "Qualidade");

    registerNode(
        makeDesc("fx.glow", "Brilho", "Filtro", "shaders/glow.frag", b.props,
                 {"Entrada"},
                 "Halo multi-escala sobre as partes claras. Mantem a cor de "
                 "cada regiao em vez de estourar tudo para branco."),
        [](ShaderProgram& p, const Node& n, Time t, const ExpressionContext& ctx) {
            setPropertyF(p, n, "u_threshold", t, ctx);
            setPropertyF(p, n, "u_intensity", t, ctx);
            setPropertyF(p, n, "u_radius", t, ctx);
            setPropertyColor(p, n, "u_tint", t, ctx);
            setPropertyI(p, n, "u_quality", t, ctx);
        });
}

void registerVignette() {
    SpecBuilder b;
    b.f("u_amount", 0.4, 0.0, 1.0, "Vinheta")
        .f("u_softness", 0.6, 0.0, 1.0, "Borda")
        .f("u_roundness", 1.0, 0.0, 1.0, "Forma", "1 = circular, 0 = retangular")
        .vec2("u_center", 0.5, 0.5, "Posicao")
        .check("u_invert", false, "Avancado", "Escurece o centro");

    registerNode(
        makeDesc("fx.vignette", "Vinheta", "Filtro", "shaders/vignette.frag", b.props,
                 {"Entrada"}, "Escurece as bordas para puxar o olho ao centro."),
        [](ShaderProgram& p, const Node& n, Time t, const ExpressionContext& ctx) {
            setPropertyF(p, n, "u_amount", t, ctx);
            setPropertyF(p, n, "u_softness", t, ctx);
            setPropertyF(p, n, "u_roundness", t, ctx);
            setPropertyV2(p, n, "u_center", t, ctx);
            setPropertyBool(p, n, "u_invert", t, ctx);
        });
}

void registerDisplace() {
    SpecBuilder b;
    b.f("u_amountX", 40.0, -1000.0, 1000.0, "Deslocamento", "Eixo horizontal, em UV")
        .f("u_amountY", 0.0, -1000.0, 1000.0, "Deslocamento", "Eixo vertical, em UV")
        .vec2("u_mapScale", 1.0, 1.0, "Mapa", "Repeticoes do mapa")
        .vec2("u_mapOffset", 0.0, 0.0, "Mapa", "Deslocamento do mapa")
        .i("u_mapChannel", 0, 0, 4, "Mapa", "0 R, 1 G, 2 B, 3 A, 4 luminancia")
        .f("u_centerBias", 0.0, 0.0, 1.0, "Forma", "Mantem o centro parado")
        .choice("u_edgeMode", "transparente",
                {kDisplaceEdgeModes, kDisplaceEdgeModes + kDisplaceEdgeModeCount},
                "Bordas")
        .check("u_invert", false, "Mapa", "Inverte o mapa")
        .f("u_turbulence", 8.0, 0.0, 64.0, "Turbulencia",
           "Sem mapa conectado, usa este ruido");

    registerNode(
        makeDesc("fx.displace", "Deslocar", "Filtro", "shaders/displace.frag", b.props,
                 {"Entrada", "Mapa"},
                 "Empurra os pixels pela informacao de um mapa (ou por "
                 "turbulencia). Base de ondas de calor, vidro e agua."),
        [](ShaderProgram& p, const Node& n, Time t, const ExpressionContext& ctx) {
            setPropertyF(p, n, "u_amountX", t, ctx);
            setPropertyF(p, n, "u_amountY", t, ctx);
            setPropertyV2(p, n, "u_mapScale", t, ctx);
            setPropertyV2(p, n, "u_mapOffset", t, ctx);
            setPropertyI(p, n, "u_mapChannel", t, ctx);
            setPropertyF(p, n, "u_centerBias", t, ctx);
            setPropertyBool(p, n, "u_invert", t, ctx);
            setPropertyF(p, n, "u_turbulence", t, ctx);
            gpu::setPropertyEnum(p, n, "u_edgeMode", t, ctx, kDisplaceEdgeModes,
                                 kDisplaceEdgeModeCount, 0);
            // A entrada 1 e o mapa; sem ela, o shader usa turbulencia.
            p.setInt("u_hasMap", 0);
        });
}

void registerChannelKey() {
    SpecBuilder b;
    b.color("u_keyColor", Color{0.0, 1.0, 0.0, 1.0}, "Cor-chave",
            "Cor a remover, em espaco linear")
        .i("u_mode", 0, 0, 1, "Metodo", "0 = similaridade, 1 = cor solida")
        .f("u_tolerance", 0.1, 0.0, 1.0, "Ajuste")
        .f("u_softness", 0.1, 0.001, 1.0, "Ajuste", "Transicao do matte")
        .f("u_choke", 0.0, -1.0, 1.0, "Matte", "Encolhe ou expande")
        .f("u_matteBlur", 0.0, 0.0, 64.0, "Matte", "Suaviza o matte, em pixels")
        .f("u_spill", 0.0, -1.0, 1.0, "Matte", "Remove a franja da cor-chave")
        .f("u_edge", 0.0, 0.0, 1.0, "Matte", "Limpa a borda")
        .check("u_invertKey", false, "Ajuste", "Remove a cor oposta");

    registerNode(
        makeDesc("fx.channelKey", "Remover fundo", "Filtro", "shaders/channel_key.frag",
                 b.props, {"Entrada"},
                 "Monta um matte a partir da cor de fundo, com controle de "
                 "tolerancia, choking, limpeza de borda e despill."),
        [](ShaderProgram& p, const Node& n, Time t, const ExpressionContext& ctx) {
            setPropertyColor(p, n, "u_keyColor", t, ctx);
            setPropertyI(p, n, "u_mode", t, ctx);
            setPropertyF(p, n, "u_tolerance", t, ctx);
            setPropertyF(p, n, "u_softness", t, ctx);
            setPropertyF(p, n, "u_choke", t, ctx);
            setPropertyF(p, n, "u_matteBlur", t, ctx);
            setPropertyF(p, n, "u_spill", t, ctx);
            setPropertyF(p, n, "u_edge", t, ctx);
            setPropertyBool(p, n, "u_invertKey", t, ctx);
        });
}

void registerMatte() {
    SpecBuilder b;
    b.choice("u_operation", "somar", {kMatteOps, kMatteOps + kMatteOpCount}, "Composicao")
        .i("u_matteSource", 0, 0, 5, "Origem", "0 alpha, 1 luminancia, 2-4 canais RGB, 5 alpha")
        .check("u_invert", false, "Matte")
        .f("u_grow", 0.0, -1.0, 1.0, "Borda", "Dilata ou corroe o matte")
        .f("u_feather", 0.0, 0.0, 64.0, "Borda", "Suavidade em pixels");

    registerNode(
        makeDesc("matte.operation", "Matte", "Filtro", "shaders/matte.frag", b.props,
                 {"Entrada", "Matte"},
                 "Usa a entrada 1 (ou o proprio alpha) como matte: uniao, "
                 "interseccao, diferenca, dilatacao e suavizacao."),
        [](ShaderProgram& p, const Node& n, Time t, const ExpressionContext& ctx) {
            setPropertyI(p, n, "u_matteSource", t, ctx);
            setPropertyBool(p, n, "u_invert", t, ctx);
            setPropertyF(p, n, "u_grow", t, ctx);
            setPropertyF(p, n, "u_feather", t, ctx);
            gpu::setPropertyEnum(p, n, "u_operation", t, ctx, kMatteOps, kMatteOpCount, 0);
        });
}

}  // namespace

void registerFilterNodes() {
    registerBlur();
    registerGlow();
    registerVignette();
    registerDisplace();
    registerChannelKey();
    registerMatte();
}

}  // namespace lmn::nodes
