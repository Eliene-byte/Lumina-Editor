#version 330 core
// glow.frag - Brilho (glow/bloom) em multi-escala.
//
// Amostra a entrada em tres niveis de raio e soma o que passa do limiar. O
// resultado e um halo suave sem o serrilhado caracteristico de um unico
// passe de blur.
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_input;
uniform vec2 u_texel;
uniform float u_threshold;   // 0..1
uniform float u_intensity;   // 0..4
uniform float u_radius;      // em pixels, no maior nivel
uniform vec3 u_tint;
uniform int u_quality;       // 0 = 3 niveis x 6 amostras, 1 = 3 x 10

// Amostra em cruz com 4 pontos, 45 graus alternados: 8 pontos com 4 texture().
vec3 tapGlow(vec2 uv, vec2 step) {
    vec3 s = texture(u_input, uv).rgb;
    s += texture(u_input, uv + step).rgb;
    s += texture(u_input, uv - step).rgb;
    s += texture(u_input, uv + vec2(step.x, -step.y)).rgb;
    s += texture(u_input, uv + vec2(-step.x, step.y)).rgb;
    if (u_quality == 1) {
        s += texture(u_input, uv + step * 1.7).rgb;
        s += texture(u_input, uv - step * 1.7).rgb;
        s += texture(u_input, uv + step * 2.6).rgb;
        s += texture(u_input, uv - step * 2.6).rgb;
        s += texture(u_input, uv + step * 3.9).rgb;
        return s / 11.0;
    }
    return s / 5.0;
}

vec3 highlightOf(vec2 uv) {
    vec3 c = texture(u_input, uv).rgb;
    float l = luma(c);
    if (l <= u_threshold) return vec3(0.0);
    // Mantem a cor do que passa do limiar em vez de trocar por branco.
    return c * ((l - u_threshold) / max(l, 1e-4));
}

void main() {
    vec4 src = texture(u_input, v_uv);
    float r = max(u_radius, 0.0);

    // Tres niveis: 1x, 3x e 8x o raio. A soma das escalas produz o halo.
    vec3 g = vec3(0.0);
    g += highlightOf(v_uv) * 0.5;
    g += tapGlow(v_uv, u_texel * r * 3.0) * 0.3;
    g += tapGlow(v_uv, u_texel * r * 8.0) * 0.2;

    // Sobrinha o halo por cima do original, com o tint aplicado.
    vec3 halo = g * u_tint * u_intensity;
    fragColor = vec4(src.rgb + halo, src.a);
}
