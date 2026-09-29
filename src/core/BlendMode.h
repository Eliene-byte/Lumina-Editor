// BlendMode.h - Modos de mesclagem entre nos.
#pragma once

#include <cstdint>

namespace lmn {

enum class BlendMode : uint8_t {
    Over = 0,       // normal / alpha sobre
    Add,
    Screen,
    Multiply,
    Overlay,
    SoftLight,
    HardLight,
    Difference,
    Exclusion,
    Subtract,
    Divide,
    Lighten,
    Darken,
    LinearBurn,
    LinearDodge,
    ColorDodge,
    ColorBurn,
    HardMix,
    // Base (Resolve/AE)
    Max,
    Min,
    // modos de canal
    Luminosity,
    Saturation,
    Hue,
    Color_,
    // modos por regiao
    StencilAlpha,
    StencilLum,
    AlphaAdd,
    Premul,
    Count,
};

const char* blendModeName(BlendMode m) noexcept;
BlendMode blendModeFromName(const char* name) noexcept;
bool blendModeUsesBackdrop(BlendMode m) noexcept;   // precisa do input 1
bool blendModeIsSeparable(BlendMode m) noexcept;   // pode usar blend 1D no shader
bool blendModeIsPerceptual(BlendMode m) noexcept;  // atua sobre luminancia/cor

}  // namespace lmn
