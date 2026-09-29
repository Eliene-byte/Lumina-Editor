#version 330 core
// brightness_contrast.frag - Brilho e contraste com o mesmo comportamento do
// After Effects: o contraste e aplicado em torno de 0.5, o que mantem a
// neutralidade em cenas de meio-tom.
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_input;
uniform float u_brightness;   // -100..100
uniform float u_contrast;     // -100..100
uniform float u_gamma;        // 0.1..10, 1 = neutro
uniform int u_useLegacy;      // 1 = curva antiga do AE (menos suave)
uniform int u_invert;

vec3 applyContrast(vec3 c, float amount, float pivot) {
    // amount em -1..1
    float k = amount >= 0.0 ? 1.0 / (1.0 - amount) : 1.0 + amount;
    return (c - pivot) * k + pivot;
}

void main() {
    vec4 src = texture(u_input, v_uv);
    vec3 c = src.rgb;

    // Brilho: o AE usa uma curva em gamma 1.9, o que da um resposta mais
    // natural que um simples +offset.
    float b = u_brightness / 100.0;
    if (abs(b) > 1e-4) {
        if (u_useLegacy == 1) {
            c = c + b;
        } else {
            float pivot = pow(0.5, 1.9);
            c = pow(max(c, 0.0), vec3(1.0 / 1.9)) * 0.5 + c * 0.5;
            c = c * (1.0 + b) - b * pivot * 0.0 + b * pivot;
        }
    }

    float k = u_contrast / 100.0;
    if (abs(k) > 1e-4) {
        float pivot = (u_useLegacy == 1) ? 0.5 : 0.18;
        c = applyContrast(c, k, pivot);
    }

    if (abs(u_gamma - 1.0) > 1e-4) {
        c = pow(max(c, 0.0), vec3(1.0 / max(u_gamma, 0.01)));
    }
    if (u_invert == 1) c = 1.0 - c;

    fragColor = vec4(c, src.a);
}
