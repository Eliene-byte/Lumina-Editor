#version 330 core
// noise_texture.frag - Geradores de textura procedural.
//
// Movimentos de fundo, texturas de Fumaca, papel, agua e Transition sem
// precisar de imagem - o que tambem mantem o projeto leve.
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform vec4 u_colorA;
uniform vec4 u_colorB;
uniform float u_scale;        // repeticiones
uniform float u_contrast;     // 0..1
uniform int u_type;           // ver tabela no no "src.noise"
uniform float u_warp;         // deformacao por ruido
uniform float u_animate;      // 0 = estatico, 1 = evolui com o tempo
uniform float u_time;
uniform int u_tiled;
uniform vec4 u_transform;     // pos.xy, escala.xy (o no "tx.set" ajusta)

// Tabela de tipos:
//  0 fBm suave   1 ridges    2 worley    3 turbulent
//  4 listras     5 xadrez     6 nebulosa  7 gradiente radial
//  8 Feuer       9 calor

float n1(vec2 p) { return vnoise(p); }

float smoke(vec2 p) {
    return fbm(p, 5, 2.0, 0.5);
}

float turbulent(vec2 p) {
    float sum = 0.0, amp = 0.5, norm = 0.0;
    for (int i = 0; i < 5; ++i) {
        sum += abs(vnoise(p) * 2.0 - 1.0) * amp;
        norm += amp;
        p *= 2.0;
        amp *= 0.5;
    }
    return sum / norm;
}

float ridges(vec2 p) {
    float sum = 0.0, amp = 0.5, norm = 0.0;
    for (int i = 0; i < 5; ++i) {
        float n = 1.0 - abs(vnoise(p) * 2.0 - 1.0);
        sum += n * n * amp;
        norm += amp;
        p *= 2.0;
        amp *= 0.5;
    }
    return sum / norm;
}

vec2 heatField(vec2 p) {
    // FeLD: distancia fracionaria iterada, o ruido mais barato que da
    // estrutura de chama convincente.
    vec2 q = p;
    float amp = 1.0;
    for (int i = 0; i < 3; ++i) {
        q += vec2(vnoise(q * 1.7 + float(i) * 3.1), vnoise(q * 1.7 + 5.2)) * 0.6;
        amp *= 0.5;
    }
    return q;
}

void main() {
    vec2 uv = v_uv;

    // Transformacao: posicao e escala em UV.
    uv = (uv - u_transform.xy) / max(u_transform.zw, vec2(1e-4)) + 0.5;
    if (u_tiled == 1) uv = fract(uv);

    float t = u_animate > 0.5 ? u_time : 0.0;
    vec2 p = uv * u_scale;

    if (u_warp > 0.0) {
        vec2 w = vec2(fbm(p + vec2(0.0, t), 3, 2.0, 0.5),
                      fbm(p + vec2(5.2, 1.3 - t), 3, 2.0, 0.5)) - 0.5;
        p += w * u_warp * 8.0;
    }

    float v = 0.5;
    if (u_type == 0)      v = smoke(p + vec2(0.0, t));
    else if (u_type == 1) v = ridges(p + vec2(t * 0.3, 0.0));
    else if (u_type == 2) v = 1.0 - worley(p + t * 0.2);
    else if (u_type == 3) v = turbulent(p + vec2(0.0, t * 0.5));
    else if (u_type == 4) {
        // Listras com bordas suaves e desvio por ruido.
        float s = sin((p.x + vnoise(p) * 0.5) * PI * 2.0);
        v = 0.5 + 0.5 * s;
    }
    else if (u_type == 5) {
        vec2 c = floor(p);
        v = mod(c.x + c.y, 2.0);
    }
    else if (u_type == 6) v = fbm(p * 0.5 + t * 0.1, 6, 2.1, 0.55);
    else if (u_type == 7) v = 1.0 - clamp(length(uv - 0.5) * 2.0, 0.0, 1.0);
    else if (u_type == 8) v = turbulent(heatField(p) + t * 0.2);
    else if (u_type == 9) v = 1.0 - clamp(length(heatField(p)) * 1.6, 0.0, 1.0);

    if (u_contrast > 0.0) v = mix(v, smoothstep(0.0, 1.0, v), u_contrast);
    v = sat(v);

    vec3 c = mix(u_colorA.rgb, u_colorB.rgb, v);
    float a = mix(u_colorA.a, u_colorB.a, v);
    fragColor = vec4(c, a);
}
