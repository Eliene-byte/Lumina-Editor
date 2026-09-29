#include "BlendMode.h"

#include <cstring>

namespace lmn {

const char* blendModeName(BlendMode m) noexcept {
    switch (m) {
        case BlendMode::Over:           return "Over";
        case BlendMode::Add:            return "Add";
        case BlendMode::Screen:         return "Screen";
        case BlendMode::Multiply:       return "Multiply";
        case BlendMode::Overlay:        return "Overlay";
        case BlendMode::SoftLight:      return "SoftLight";
        case BlendMode::HardLight:      return "HardLight";
        case BlendMode::Difference:     return "Difference";
        case BlendMode::Exclusion:      return "Exclusion";
        case BlendMode::Subtract:       return "Subtract";
        case BlendMode::Divide:         return "Divide";
        case BlendMode::Lighten:        return "Lighten";
        case BlendMode::Darken:         return "Darken";
        case BlendMode::LinearBurn:     return "LinearBurn";
        case BlendMode::LinearDodge:    return "LinearDodge";
        case BlendMode::ColorDodge:     return "ColorDodge";
        case BlendMode::ColorBurn:      return "ColorBurn";
        case BlendMode::HardMix:        return "HardMix";
        case BlendMode::Max:            return "Max";
        case BlendMode::Min:            return "Min";
        case BlendMode::Luminosity:     return "Luminosity";
        case BlendMode::Saturation:     return "Saturation";
        case BlendMode::Hue:            return "Hue";
        case BlendMode::Color_:         return "Color";
        case BlendMode::StencilAlpha:   return "StencilAlpha";
        case BlendMode::StencilLum:     return "StencilLum";
        case BlendMode::AlphaAdd:       return "AlphaAdd";
        case BlendMode::Premul:         return "Premul";
        case BlendMode::Count:          break;
    }
    return "Over";
}

BlendMode blendModeFromName(const char* name) noexcept {
    if (!name) return BlendMode::Over;
    for (uint8_t i = 0; i < static_cast<uint8_t>(BlendMode::Count); ++i) {
        if (std::strcmp(name, blendModeName(static_cast<BlendMode>(i))) == 0) {
            return static_cast<BlendMode>(i);
        }
    }
    return BlendMode::Over;
}

bool blendModeUsesBackdrop(BlendMode m) noexcept {
    switch (m) {
        case BlendMode::Over:
        case BlendMode::Add:
        case BlendMode::Screen:
        case BlendMode::Multiply:
        case BlendMode::Overlay:
        case BlendMode::SoftLight:
        case BlendMode::HardLight:
        case BlendMode::Difference:
        case BlendMode::Exclusion:
        case BlendMode::Subtract:
        case BlendMode::Divide:
        case BlendMode::Lighten:
        case BlendMode::Darken:
        case BlendMode::LinearBurn:
        case BlendMode::LinearDodge:
        case BlendMode::ColorDodge:
        case BlendMode::ColorBurn:
        case BlendMode::HardMix:
        case BlendMode::Max:
        case BlendMode::Min:
        case BlendMode::Luminosity:
        case BlendMode::Saturation:
        case BlendMode::Hue:
        case BlendMode::Color_:
        case BlendMode::StencilAlpha:
        case BlendMode::StencilLum:
        case BlendMode::AlphaAdd:
            return true;
        case BlendMode::Premul:
        case BlendMode::Count:
            return false;
    }
    return false;
}

bool blendModeIsSeparable(BlendMode m) noexcept {
    switch (m) {
        case BlendMode::Over:
        case BlendMode::Add:
        case BlendMode::Screen:
        case BlendMode::Multiply:
        case BlendMode::Overlay:
        case BlendMode::SoftLight:
        case BlendMode::HardLight:
        case BlendMode::Difference:
        case BlendMode::Exclusion:
        case BlendMode::LinearBurn:
        case BlendMode::LinearDodge:
        case BlendMode::ColorDodge:
        case BlendMode::ColorBurn:
        case BlendMode::HardMix:
        case BlendMode::Max:
        case BlendMode::Min:
        case BlendMode::Lighten:
        case BlendMode::Darken:
            return true;
        case BlendMode::Subtract:
        case BlendMode::Divide:
        case BlendMode::Luminosity:
        case BlendMode::Saturation:
        case BlendMode::Hue:
        case BlendMode::Color_:
        case BlendMode::StencilAlpha:
        case BlendMode::StencilLum:
        case BlendMode::AlphaAdd:
        case BlendMode::Premul:
        case BlendMode::Count:
            return false;
    }
    return false;
}

bool blendModeIsPerceptual(BlendMode m) noexcept {
    switch (m) {
        case BlendMode::Luminosity:
        case BlendMode::Saturation:
        case BlendMode::Hue:
        case BlendMode::Color_:
            return true;
        default:
            return false;
    }
}

}  // namespace lmn
