// NodeHelpers.h - Atalhos para declarar nos com menos repeticao.
//
// Um no e um NodeTypeDesc (propriedades, entradas, shader) mais um binder de
// uniforms. Este arquivo encurta a declaracao dos dois.
#pragma once

#include <string>
#include <vector>

#include "core/NodeRegistry.h"
#include "core/Property.h"
#include "gpu/NodeUniforms.h"

namespace lmn::nodes {

using gpu::UniformBinder;

struct SpecBuilder {
    std::vector<PropertySpec> props;

    SpecBuilder& f(const char* name, double def, double lo, double hi,
                   const char* group = "", const char* tip = "",
                   bool clamps = false) {
        PropertySpec p;
        p.name = name;
        p.type = ValueType::Double;
        p.defaultValue = Value(def);
        p.ui = PropertyUi::Slider;
        p.min = lo;
        p.max = hi;
        p.animatable = true;
        p.clamps = clamps;
        p.group = group;
        p.tooltip = tip;
        props.push_back(std::move(p));
        return *this;
    }

    SpecBuilder& i(const char* name, double def, double lo, double hi,
                   const char* group = "", const char* tip = "") {
        f(name, def, lo, hi, group, tip);
        props.back().type = ValueType::Double;
        props.back().ui = PropertyUi::Default;
        return *this;
    }

    SpecBuilder& check(const char* name, bool def, const char* group = "",
                       const char* tip = "") {
        PropertySpec p;
        p.name = name;
        p.type = ValueType::Bool;
        p.defaultValue = Value(def);
        p.ui = PropertyUi::Checkbox;
        p.animatable = false;
        p.group = group;
        p.tooltip = tip;
        props.push_back(std::move(p));
        return *this;
    }

    SpecBuilder& color(const char* name, const Color& def, const char* group = "",
                       const char* tip = "") {
        PropertySpec p;
        p.name = name;
        p.type = ValueType::Color;
        p.defaultValue = Value(def);
        p.ui = PropertyUi::Color;
        p.min = 0.0;
        p.max = 1.0;
        p.clamps = true;
        p.group = group;
        p.tooltip = tip;
        props.push_back(std::move(p));
        return *this;
    }

    SpecBuilder& vec2(const char* name, double x, double y, const char* group = "",
                      const char* tip = "") {
        PropertySpec p;
        p.name = name;
        p.type = ValueType::Vec2;
        p.defaultValue = Value(Vec2{x, y});
        p.ui = PropertyUi::Position;
        p.min = -1e5;
        p.max = 1e5;
        p.group = group;
        p.tooltip = tip;
        props.push_back(std::move(p));
        return *this;
    }

    SpecBuilder& vec4(const char* name, const Vec4& def, const char* group = "",
                      const char* tip = "") {
        PropertySpec p;
        p.name = name;
        p.type = ValueType::Vec4;
        p.defaultValue = Value(def);
        p.ui = PropertyUi::Default;
        p.group = group;
        p.tooltip = tip;
        props.push_back(std::move(p));
        return *this;
    }

    SpecBuilder& angle(const char* name, double def, const char* group = "",
                       const char* tip = "") {
        PropertySpec p;
        p.name = name;
        p.type = ValueType::Double;
        p.defaultValue = Value(def);
        p.ui = PropertyUi::Angle;
        p.min = -1e5;
        p.max = 1e5;
        p.group = group;
        p.tooltip = tip;
        props.push_back(std::move(p));
        return *this;
    }

    SpecBuilder& text(const char* name, const char* def, const char* group = "",
                      const char* tip = "") {
        PropertySpec p;
        p.name = name;
        p.type = ValueType::String;
        p.defaultValue = Value(std::string(def));
        p.ui = PropertyUi::Text;
        p.animatable = false;
        p.group = group;
        p.tooltip = tip;
        props.push_back(std::move(p));
        return *this;
    }

    SpecBuilder& file(const char* name, const char* def, const char* group = "",
                      const char* tip = "") {
        text(name, def, group, tip);
        props.back().ui = PropertyUi::FilePath;
        return *this;
    }

    SpecBuilder& choice(const char* name, const char* def,
                        const std::vector<std::string>& options, const char* group = "",
                        const char* tip = "") {
        PropertySpec p;
        p.name = name;
        p.type = ValueType::String;
        p.defaultValue = Value(std::string(def));
        p.ui = PropertyUi::Enum;
        p.animatable = false;
        p.options = options;
        p.group = group;
        p.tooltip = tip;
        props.push_back(std::move(p));
        return *this;
    }

    // Identico a 'f', mas com expressao desligada por padrao (valores que
    // normalmente nao fazem sentido animados).
    SpecBuilder& fixed(const char* name, double def, const char* group = "") {
        f(name, def, -1e6, 1e6, group, "");
        props.back().expressionEnabled = false;
        return *this;
    }
};

NodeTypeDesc makeDesc(std::string type, std::string displayName, std::string category,
                      std::string shader, std::vector<PropertySpec> props,
                      std::vector<std::string> inputs, std::string description = {});

// Registra o tipo e o binder juntos. Todo no novo passa por aqui.
void registerNode(NodeTypeDesc desc, UniformBinder binder = {});

// --- listas de opcoes compartilhadas ---------------------------------------
extern const char* const kGradientTypes[];
extern const int kGradientTypeCount;
extern const char* const kNoiseTypes[];
extern const int kNoiseTypeCount;
extern const char* const kShapeTypes[];
extern const int kShapeTypeCount;
extern const char* const kMatteOps[];
extern const int kMatteOpCount;
extern const char* const kBlendModes[];
extern const int kBlendModeCount;
extern const char* const kDisplayTransforms[];
extern const int kDisplayTransformCount;
extern const char* const kTimeStretchModes[];
extern const int kTimeStretchModeCount;
extern const char* const kDisplaceEdgeModes[];
extern const int kDisplaceEdgeModeCount;
extern const char* const kLutDomains[];
extern const int kLutDomainCount;
extern const char* const kColorBalanceModes[];
extern const int kColorBalanceModeCount;

}  // namespace lmn::nodes
