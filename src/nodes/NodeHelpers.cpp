#include "NodeHelpers.h"

namespace lmn::nodes {

// Ordem = indice enviado ao shader. Alterar aqui exige alterar o switch no
// .frag correspondente; cada lista tem um comentario apontando o shader.
const char* const kGradientTypes[] = {"solido", "linear", "radial", "angular",
                                      "conico", "losango"};
const int kGradientTypeCount = 6;                    // solid.frag

const char* const kNoiseTypes[] = {"fbm suave", "ridges", "worley", "turbulento",
                                   "listras", "xadrez", "nebulosa", "radial",
                                   "chama", "calor"};
const int kNoiseTypeCount = 10;                      // noise_texture.frag

const char* const kShapeTypes[] = {"retangulo", "elipse", "triangulo", "estrela",
                                   "poligono"};
const int kShapeTypeCount = 5;                       // shape.frag

const char* const kMatteOps[] = {"somar", "subtrair", "intersecao", "uniao",
                                 "inverter", "maximo"};
const int kMatteOpCount = 6;                         // matte.frag

const char* const kDisplayTransforms[] = {"Standard (sRGB)", "Rec.709", "P3", "HLG",
                                          "ACES", "Linear (sem conversao)"};
const int kDisplayTransformCount = 6;                // view_transform.frag

const char* const kTimeStretchModes[] = {"segurar", "linear", "borrado"};
const int kTimeStretchModeCount = 3;                 // time_stretch.frag

const char* const kDisplaceEdgeModes[] = {"transparente", "repetir", "limitar",
                                          "espelhar"};
const int kDisplaceEdgeModeCount = 4;                // displace.frag

const char* const kLutDomains[] = {"Log", "Linear", "Video"};
const int kLutDomainCount = 3;                       // lut3d.frag

const char* const kColorBalanceModes[] = {"Lift/Gamma/Gain", "Offset/Contraste"};
const int kColorBalanceModeCount = 2;                // color_balance.frag

NodeTypeDesc makeDesc(std::string type, std::string displayName, std::string category,
                      std::string shader, std::vector<PropertySpec> props,
                      std::vector<std::string> inputs, std::string description) {
    NodeTypeDesc d;
    d.type = std::move(type);
    d.displayName = std::move(displayName);
    d.category = std::move(category);
    d.shader = std::move(shader);
    d.properties = std::move(props);
    d.inputNames = std::move(inputs);
    d.outputNames = {"Saida"};
    d.description = std::move(description);
    d.isGenerator = d.inputCount() == 0;
    d.isFilter = d.inputCount() == 1;
    d.isMerger = d.inputCount() > 1;
    return d;
}

void registerNode(NodeTypeDesc desc, UniformBinder binder) {
    const std::string type = desc.type;
    if (binder) gpu::NodeUniformRegistry::instance().registerBinder(type, binder);
    NodeRegistry::instance().registerType(desc);
}

}  // namespace lmn::nodes
