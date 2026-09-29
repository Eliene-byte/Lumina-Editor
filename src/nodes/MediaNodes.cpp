// MediaNodes.cpp - Nos que trazem midia de fora para o grafo.
//
// Estes nos sao os unicos que dependem da camada de decodificacao. Sem um
// provedor de fonte registrado, eles devolvem nullptr e o compositor usa o
// fallback, o que mantem o editor utilizavel enquanto o FFmpeg nao carrega.
#include "NodeHelpers.h"

#include "ImageCache.h"

namespace lmn::nodes {
namespace {

using gpu::ShaderProgram;
using gpu::setPropertyF;
using gpu::setPropertyI;
using gpu::setPropertyV2;
using gpu::setPropertyV4;

void registerImage() {
    SpecBuilder b;
    b.file("path", "", "Arquivo", "Imagem ou sequence")
        .f("u_frame", 0.0, 0.0, 100000.0, "Arquivo", "Quadro de uma sequence")
        .vec2("u_offset", 0.0, 0.0, "Enquadramento", "Desloca a imagem na composicao")
        .f("u_zoom", 100.0, 0.01, 10000.0, "Enquadramento", "Escala em %")
        .check("u_fitMode", true, "Enquadramento", "Ajusta a imagem na composicao")
        .check("u_pixelAspect", true, "Enquadramento", "Corrige aspecto nao quadrado")
        .check("u_premultiply", true, "Cor", "Multiplica a cor pelo alpha");

    NodeTypeDesc d = makeDesc("src.image", "Imagem", "Fonte", "shaders/copy.frag",
                              b.props, {},
                              "Carrega uma imagem, PSD ou sequence. O arquivo e "
                              "lido uma vez e fica em cache.");
    d.hasTransform = false;   // o enquadramento e feito pelas propriedades
    d.description = "Imagem, PSD ou sequence de quadros.";

    registerNode(d, [](ShaderProgram& p, const Node& n, Time t,
                       const ExpressionContext& ctx) {
        setPropertyV2(p, n, "u_offset", t, ctx);
        setPropertyF(p, n, "u_zoom", t, ctx);
        // A textura em si e vinculada pelo SourceProvider, que resolve o
        // caminho e o upload para a GPU.
        p.setInt("u_useTransform", 0);
        p.setInt("u_hasInput", 0);
        p.setFloat("u_opacity", 1.0f);
    });
}

void registerMediaClip() {
    SpecBuilder b;
    b.f("mediaId", 0.0, 0.0, 1e9, "Midia", "Item do acervo")
        .f("u_startTime", 0.0, -1e6, 1e6, "Tempo", "Inicio do trecho na origem")
        .f("u_playbackRate", 1.0, 0.01, 20.0, "Tempo", "Velocidade de reproducao")
        .f("u_reverse", 0.0, 0.0, 1.0, "Tempo", "1 = reproduz ao contrario")
        .f("u_loop", 0.0, 0.0, 1.0, "Tempo", "Repete ao chegar no fim")
        .check("u_useProxy", true, "Desempenho", "Usa o proxy leve quando existir")
        .f("u_fitMode", 0.0, 0.0, 3.0, "Enquadramento",
           "0 = tamanho real, 1 = caber, 2 = preencher, 3 = esticar")
        .f("u_fitScale", 100.0, 1.0, 400.0, "Enquadramento")
        .vec2("u_offset", 0.0, 0.0, "Enquadramento")
        .angle("u_rotation", 0.0, "Enquadramento");

    NodeTypeDesc d = makeDesc("src.media", "Clip de midia", "Fonte", "shaders/copy.frag",
                              b.props, {},
                              "Video, audio ou imagem do acervo, com velocidade, "
                              "inversao e loop. Decodificacao fica com media/.");
    d.hasTransform = false;
    d.hasMedia = true;

    registerNode(d, [](ShaderProgram& p, const Node& n, Time t,
                       const ExpressionContext& ctx) {
        setPropertyF(p, n, "u_opacity", 1.0f);
        setPropertyV2(p, n, "u_offset", t, ctx);
        p.setInt("u_useTransform", 0);
    });
}

void registerCompositionRef() {
    SpecBuilder b;
    b.f("composition", 0.0, 0.0, 1e9, "Composicao", "Qual composicao usar")
        .f("u_timeOffset", 0.0, -1e6, 1e6, "Tempo", "Desloca o tempo da composicao")
        .check("u_loop", false, "Tempo", "Repete alem da duracao")
        .check("u_isolate", false, "Avancado", "Renderiza em um contexto separado");

    NodeTypeDesc d = makeDesc("comp.reference", "Composicao", "Fonte",
                              "shaders/copy.frag", b.props, {},
                              "Aninha outra composicao, como uma camada com "
                              "pre-composicao no After Effects.");
    d.hasTransform = false;
    d.hasCompositionRef = true;

    registerNode(d, [](ShaderProgram& p, const Node& n, Time t,
                       const ExpressionContext& ctx) {
        setPropertyF(p, n, "u_timeOffset", t, ctx);
        p.setInt("u_useTransform", 0);
        p.setFloat("u_opacity", 1.0f);
    });
}

void registerAudioMeter() {
    SpecBuilder b;
    b.f("audioId", 0.0, 0.0, 1e9, "Audio")
        .f("u_threshold", -60.0, -100.0, 0.0, "Medidor", "Nivel em dBFS");

    // Nao produz imagem: existe para o grafo descrever o que existe de audio,
    // e o painel de medidores ler daqui.
    NodeTypeDesc d = makeDesc("audio.clip", "Audio", "Audio", "shaders/copy.frag",
                              b.props, {}, "Clip de audio. Nao afeta a imagem.");
    d.hasTransform = false;
    d.isAudio = true;

    registerNode(d, [](ShaderProgram&, const Node&, Time, const ExpressionContext&) {});
}

}  // namespace

void registerMediaNodes() {
    registerImage();
    registerMediaClip();
    registerCompositionRef();
    registerAudioMeter();
}

}  // namespace lmn::nodes
