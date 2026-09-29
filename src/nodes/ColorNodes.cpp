// ColorNodes.cpp - Nos de cor: brilho, curvas, matiz, balance, LUT e o no
// final que converte para o display.
#include "NodeHelpers.h"

#include <array>
#include <cstdlib>

namespace lmn::nodes {
namespace {

using gpu::ShaderProgram;
using gpu::setPropertyBool;
using gpu::setPropertyColor;
using gpu::setPropertyF;
using gpu::setPropertyI;
using gpu::setPropertyV2;
using gpu::setPropertyV3;

constexpr int kMaxCurvePoints = 8;

void registerBrightnessContrast() {
    SpecBuilder b;
    b.f("u_brightness", 0.0, -100.0, 100.0, "Brilho")
        .f("u_contrast", 0.0, -100.0, 100.0, "Contraste")
        .f("u_gamma", 1.0, 0.1, 4.0, "Gamma")
        .check("u_invert", false, "Avancado", "Inverte as cores")
        .check("u_useLegacy", false, "Avancado",
               "Usa a curva antiga do After Effects");

    registerNode(
        makeDesc("cc.brightnessContrast", "Brilho e Contraste", "Cor",
                 "shaders/brightness_contrast.frag", b.props, {"Entrada"},
                 "Ajuste primario de luz. O contraste age em torno de 0.18, "
                 "como no After Effects."),
        [](ShaderProgram& p, const Node& n, Time t, const ExpressionContext& ctx) {
            setPropertyF(p, n, "u_brightness", t, ctx);
            setPropertyF(p, n, "u_contrast", t, ctx);
            setPropertyF(p, n, "u_gamma", t, ctx);
            setPropertyBool(p, n, "u_invert", t, ctx);
            setPropertyBool(p, n, "u_useLegacy", t, ctx);
        });
}

void registerHueSaturation() {
    SpecBuilder b;
    b.f("u_hue", 0.0, -180.0, 180.0, "Matiz")
        .f("u_saturation", 0.0, -100.0, 100.0, "Saturacao")
        .f("u_lightness", 0.0, -100.0, 100.0, "Luminosidade")
        .f("u_colorize", 0.0, 0.0, 1.0, "Colorizar", "0 = off")
        .f("u_colorizeHue", 0.0, 0.0, 360.0, "Colorizar")
        .f("u_colorizeSat", 25.0, 0.0, 100.0, "Colorizar")
        .check("u_useAdobe", true, "Avancado", "Usa a curva do After Effects");

    registerNode(
        makeDesc("cc.hueSaturation", "Matiz / Saturacao", "Cor",
                 "shaders/hue_saturation.frag", b.props, {"Entrada"},
                 "Gira o matiz, mexe na saturacao e na luminosidade, ou coloriza "
                 "a imagem inteira."),
        [](ShaderProgram& p, const Node& n, Time t, const ExpressionContext& ctx) {
            setPropertyF(p, n, "u_hue", t, ctx);
            setPropertyF(p, n, "u_saturation", t, ctx);
            setPropertyF(p, n, "u_lightness", t, ctx);
            setPropertyF(p, n, "u_colorize", t, ctx);
            setPropertyF(p, n, "u_colorizeHue", t, ctx);
            setPropertyF(p, n, "u_colorizeSat", t, ctx);
            setPropertyBool(p, n, "u_useAdobe", t, ctx);
        });
}

void registerCurves() {
    // Os pontos de controle ficam num texto no formato "x,y;x,y;x,y" por canal.
    // O grafico de curvas do inspector escreve nesse texto e o binder converte
    // para o array de float que o shader consome. Guardar como texto (e nao
    // como Property numerica) mantem a propriedade animavel desativada, como
    // uma curva nao deve ser.
    SpecBuilder b;
    b.text("u_master", "", "Curva master", "Pontos de controle")
        .text("u_red", "", "Canal vermelho")
        .text("u_green", "", "Canal verde")
        .text("u_blue", "", "Canal azul");

    registerNode(
        makeDesc("cc.curves", "Curvas", "Cor", "shaders/curves.frag", b.props,
                 {"Entrada"},
                 "Curva de correcao por canal. Os pontos sao editaveis no "
                 "grafico de curvas do inspector."),
        [](ShaderProgram& p, const Node& n, Time t, const ExpressionContext& ctx) {
            struct Curve {
                int count = 0;
                std::array<float, kMaxCurvePoints * 2> points{};
            };

            auto readCurve = [&](const char* propName) {
                Curve c;
                const Property* prop = n.findProperty(propName);
                if (!prop) return c;
                const std::string raw = prop->evaluate(t, ctx).asString();

                size_t start = 0;
                while (start < raw.size() && c.count < kMaxCurvePoints) {
                    const size_t sep = raw.find(';', start);
                    const std::string pair = raw.substr(
                        start, sep == std::string::npos ? std::string::npos : sep - start);
                    const size_t comma = pair.find(',');
                    if (comma != std::string::npos) {
                        c.points[static_cast<size_t>(c.count) * 2] =
                            static_cast<float>(std::atof(pair.substr(0, comma).c_str()));
                        c.points[static_cast<size_t>(c.count) * 2 + 1] =
                            static_cast<float>(std::atof(pair.substr(comma + 1).c_str()));
                        ++c.count;
                    }
                    if (sep == std::string::npos) break;
                    start = sep + 1;
                }
                return c;
            };

            const Curve master = readCurve("u_master");
            const Curve red = readCurve("u_red");
            const Curve green = readCurve("u_green");
            const Curve blue = readCurve("u_blue");

            p.setInt("u_masterCount", master.count);
            p.setFloatArray("u_master", master.points.data(), master.count * 2);
            p.setInt("u_redCount", red.count);
            p.setFloatArray("u_red", red.points.data(), red.count * 2);
            p.setInt("u_greenCount", green.count);
            p.setFloatArray("u_green", green.points.data(), green.count * 2);
            p.setInt("u_blueCount", blue.count);
            p.setFloatArray("u_blue", blue.points.data(), blue.count * 2);
        });
}

void registerColorBalance() {
    SpecBuilder b;
    b.vec4("u_lift", Vec4{0.0, 0.0, 0.0, 0.0}, "Lift", "Desloca as sombras por canal")
        .vec4("u_gamma", Vec4{1.0, 1.0, 1.0, 1.0}, "Gamma", "Meio-tom por canal")
        .vec4("u_gain", Vec4{1.0, 1.0, 1.0, 1.0}, "Gain", "Altas luzes por canal")
        .f("u_offset", 0.0, -1.0, 1.0, "Geral")
        .f("u_contrast", 0.0, -1.0, 1.0, "Geral")
        .f("u_saturation", 0.0, -1.0, 1.0, "Geral")
        .f("u_pivot", 0.18, 0.0, 1.0, "Geral", "Luminancia em torno da qual o "
                                               "contraste age")
        .choice("u_useLGG", "Lift/Gamma/Gain",
                {kColorBalanceModes, kColorBalanceModes + kColorBalanceModeCount},
                "Modo");

    registerNode(
        makeDesc("cc.colorBalance", "Lift / Gamma / Gain", "Cor",
                 "shaders/color_balance.frag", b.props, {"Entrada"},
                 "Correcao de cor por faixa tonal, no estilo DaVinci Resolve. "
                 "O jeito mais rapido de casar dois clipes."),
        [](ShaderProgram& p, const Node& n, Time t, const ExpressionContext& ctx) {
            setPropertyV3(p, n, "u_lift", t, ctx);
            setPropertyV3(p, n, "u_gamma", t, ctx);
            setPropertyV3(p, n, "u_gain", t, ctx);
            setPropertyF(p, n, "u_offset", t, ctx);
            setPropertyF(p, n, "u_contrast", t, ctx);
            setPropertyF(p, n, "u_saturation", t, ctx);
            setPropertyF(p, n, "u_pivot", t, ctx);
            gpu::setPropertyEnum(p, n, "u_useLGG", t, ctx, kColorBalanceModes,
                                 kColorBalanceModeCount, 0);
        });
}

void registerLut() {
    SpecBuilder b;
    b.file("u_lutPath", "", "LUT", "Arquivo .cube")
        .f("u_intensity", 1.0, 0.0, 1.0, "LUT", "Mistura entre original e LUT")
        .f("u_size", 33.0, 2.0, 65.0, "Avancado", "Resolucao do cubo")
        .f("u_tilesPerRow", 6.0, 1.0, 16.0, "Avancado", "Tiles por linha na textura")
        .choice("u_domain", "Log", {kLutDomains, kLutDomains + kLutDomainCount},
                "LUT", "Espaco de entrada da LUT");

    registerNode(
        makeDesc("cc.lut3d", "LUT 3D", "Cor", "shaders/lut3d.frag", b.props,
                 {"Entrada"},
                 "Aplica uma tabela de cores .cube. O carregamento real do "
                 "arquivo fica em media/Lut3D.cpp."),
        [](ShaderProgram& p, const Node& n, Time t, const ExpressionContext& ctx) {
            setPropertyF(p, n, "u_intensity", t, ctx);
            setPropertyF(p, n, "u_size", t, ctx);
            setPropertyF(p, n, "u_tilesPerRow", t, ctx);
            gpu::setPropertyEnum(p, n, "u_domain", t, ctx, kLutDomains,
                                 kLutDomainCount, 0);
            // A textura e vinculada pelo SourceProvider; sem arquivo, o no
            // repassa a entrada sem alteracao.
            p.setInt("u_hasLut", 0);
        });
}

void registerViewTransform() {
    SpecBuilder b;
    b.choice("u_display", "Rec.709",
             {kDisplayTransforms, kDisplayTransforms + kDisplayTransformCount},
             "Display", "Como a imagem linear vira pixels na tela")
        .f("u_exposure", 0.0, -10.0, 10.0, "Ajuste", "Exposicao em stops")
        .f("u_gamma", 1.0, 0.1, 4.0, "Ajuste")
        .vec4("u_gain", Vec4{1.0, 1.0, 1.0, 1.0}, "Ajuste", "Ganho por canal")
        .vec4("u_lift", Vec4{0.0, 0.0, 0.0, 0.0}, "Ajuste", "Preto por canal")
        .f("u_saturation", 1.0, 0.0, 3.0, "Ajuste")
        .check("u_bypass", false, "Avancado", "Mostra o valor linear cru");

    NodeTypeDesc d = makeDesc("color.viewTransform", "Transformar para Display", "Cor",
                              "shaders/view_transform.frag", b.props, {"Entrada"},
                              "Converte de linear para o espaco da tela. Deve ser o "
                              "ultimo no antes de exportar.");
    d.hasTransform = false;   // a transformacao de camada aqui quebraria o display
    registerNode(d, [](ShaderProgram& p, const Node& n, Time t,
                       const ExpressionContext& ctx) {
        setPropertyF(p, n, "u_exposure", t, ctx);
        setPropertyF(p, n, "u_gamma", t, ctx);
        setPropertyF(p, n, "u_saturation", t, ctx);
        setPropertyBool(p, n, "u_bypass", t, ctx);
        if (const Property* g = n.findProperty("u_gain")) {
            const Vec4 v = g->evaluate(t, ctx).toVec4();
            p.setVec3("u_gain", Vec3{v.x, v.y, v.z});
        }
        if (const Property* l = n.findProperty("u_lift")) {
            const Vec4 v = l->evaluate(t, ctx).toVec4();
            p.setVec3("u_lift", Vec3{v.x, v.y, v.z});
        }
        gpu::setPropertyEnum(p, n, "u_display", t, ctx, kDisplayTransforms,
                             kDisplayTransformCount, 1);
    });
}

}  // namespace

void registerColorNodes() {
    registerBrightnessContrast();
    registerHueSaturation();
    registerCurves();
    registerColorBalance();
    registerLut();
    registerViewTransform();
}

}  // namespace lmn::nodes
