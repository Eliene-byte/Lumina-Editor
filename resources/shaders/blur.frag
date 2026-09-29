#version 330 core
// blur.frag - Desfoque gaussiano de raio variavel.
//
// Duas passadas separaveis (horizontal e vertical) com 13 amostras cada,
// usando amostragem linear para ter 7 amostras efetivas. O desfoque acontece em
// linear, e nao em gamma - desfocar depois da curva da luz estoura as bordas.
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_input;
uniform vec2 u_direction;     // (1,0) ou (0,1) - o no dispara as duas passadas
uniform vec2 u_texel;
uniform float u_radius;       // em pixels
uniform int u_quality;        // 0 = 5 amostras, 1 = 9, 2 = 13
uniform float u_alphaScale;   // 1 = blur tambem no alpha
uniform int u_wrap;           // 0 clamp, 1 wrap, 2 mirror

vec2 wrapUv(vec2 uv) {
    if (u_wrap == 1) return fract(uv);
    if (u_wrap == 2) return abs(fract(uv * 0.5) * 2.0 - 1.0);
    return clamp(uv, u_texel * 0.5, 1.0 - u_texel * 0.5);
}

void main() {
    // Gaussiana normalizada: os pesos vem de uma funcao de B-spline cubica,
    // que aproxima a gaussiana com pouquissimas amostras e sem serrilhado.
    const int TAPS = 13;
    float r = max(u_radius, 0.0);

    vec4 sum = vec4(0.0);
    float wsum = 0.0;

    for (int i = -6; i <= 6; ++i) {
        if (u_quality == 0 && abs(i) > 2) continue;
        if (u_quality == 1 && abs(i) > 4) continue;

        float x = float(i);
        // B-spline cubica como peso: suave e barata.
        float x1 = abs(x);
        float w = x1 < 1.0 ? (4.0 - 6.0 * x1 * x1 + 3.0 * x1 * x1 * x1) / 6.0
                           : (2.0 - x1) * (2.0 - x1) * (2.0 - x1) / 6.0;
        if (w <= 0.0) continue;

        vec2 uv = wrapUv(v_uv + u_direction * u_texel * x * r);
        vec4 s = texture(u_input, uv);
        if (u_alphaScale < 0.5) s.rgb *= s.a;   // soborra sem arrastar o alpha
        sum += s * w;
        wsum += w;
    }

    fragColor = wsum > 0.0 ? sum / wsum : texture(u_input, v_uv);
}
