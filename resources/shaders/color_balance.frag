#version 330 core
// color_balance.frag - Lift / Gamma / Gain no estilo DaVinci Resolve.
//
// Lift desloca as sombras, Gain as altas luzes e Gamma o meio-tom. E a forma
// mais rapida de casar dois clipes sem mexer em curva.
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_input;
uniform vec3 u_lift;        // -1..1 por canal
uniform vec3 u_gamma;       // 0.1..4, 1 = neutro
uniform vec3 u_gain;        // 0..4, 1 = neutro
uniform float u_offset;     // -1..1, desloca tudo
uniform float u_contrast;   // -1..1
uniform float u_saturation; // -1..1
uniform float u_pivot;      // luminancia em torno da qual o contraste age
uniform int u_useLGG;       // 1 = lift/gamma/gain, 0 = so offset/contraste

void main() {
    vec4 src = texture(u_input, v_uv);
    vec3 c = clamp(src.rgb, 0.0, 1.0);

    if (u_useLGG == 1) {
        c = c * u_gain + u_lift;
        c = pow(max(c, 0.0), 1.0 / max(u_gamma, vec3(0.01)));
    }

    c = c + u_offset;

    if (abs(u_contrast) > 1e-4) {
        float k = u_contrast >= 0.0 ? 1.0 / (1.0 - u_contrast) : 1.0 + u_contrast;
        c = (c - u_pivot) * k + u_pivot;
    }

    if (abs(u_saturation) > 1e-4) {
        c = mix(vec3(luma(c)), c, 1.0 + u_saturation);
    }

    fragColor = vec4(max(c, 0.0), src.a);
}
