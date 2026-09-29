#version 330 core
// channel_key.frag - Removedor de fundo por canal, no estilo Difference/Simple
// Keyer do AE.
//
// Isola a cor de fundo e usa o alpha resultante como matte, com desfoque,
// amplia��ao de matte e borda limpa.
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_input;
uniform vec4 u_keyColor;
uniform int u_mode;            // 0 similaridade, 1 cor solida, 2 luminancia
uniform float u_tolerance;
uniform float u_softness;      // 0 = corte duro
uniform float u_choke;         // encolhe o matte
uniform float u_matteBlur;     // em pixels
uniform float u_spill;         // -1..1, remove a franja da cor-chave
uniform float u_edge;          // limpa a borda
uniform vec2 u_texel;
uniform int u_invertKey;

vec3 despill(vec3 c) {
    if (abs(u_spill) < 1e-3) return c;
    // A cor-chave deixa o pixel com o canal do fundo saturado. Puxar esse
    // canal de volta para a media dos outros remove a franja.
    float m = max(c.r, max(c.g, c.b));
    if (m < 1e-4) return c;
    if (u_spill > 0.0) {
        c = mix(c, vec3(min(m, luma(c) + 0.5)), u_spill);
    } else {
        // Negativo: mantem so o que sobra acima da media.
        float keep = sat((m - luma(c)) / max(m - luma(c), 1e-4));
        c *= mix(1.0, keep, -u_spill);
    }
    return c;
}

float keyAlpha(vec3 c) {
    if (u_mode == 1) {
        // Cor solida: a distancia ate a cor-chave vira alpha direto.
        vec3 d = abs(c - u_keyColor.rgb);
        return 1.0 - sat(max(d.r, max(d.g, d.b)) / max(u_tolerance, 1e-4));
    }
    // Similaridade por distancia, normalizada pela luminancia da cor-chave,
    // o que evita que a tolerancia dependa do brilho do fundo.
    float keyL = max(luma(u_keyColor.rgb), 1e-3);
    float dist = length(c - u_keyColor.rgb) / (keyL * 2.0);
    return 1.0 - sat((dist - u_tolerance) / max(u_softness, 1e-4));
}

void main() {
    vec4 src = texture(u_input, v_uv);
    float a = keyAlpha(src.rgb);
    if (u_invertKey == 1) a = 1.0 - a;

    // Choke: encolhe (ou expande) o matte em torno de 0.5.
    a = sat((a - 0.5) * (1.0 / max(1.0 - u_choke, 1e-3)) + 0.5);

    // Desfoque do matte, em coordenadas de UV.
    if (u_matteBlur > 0.01) {
        float sum = 0.0;
        const float W[5] = float[](0.227, 0.194, 0.121, 0.054, 0.016);
        sum += a * W[0];
        for (int i = 1; i < 5; ++i) {
            vec2 d = u_texel * float(i) * u_matteBlur;
            sum += keyAlpha(texture(u_input, clamp(v_uv + d, 0.0, 1.0)).rgb) * W[i];
            sum += keyAlpha(texture(u_input, clamp(v_uv - d, 0.0, 1.0)).rgb) * W[i];
        }
        a = sum;
    }

    // Limpeza de borda: reforca a transicao alpha para evitar franja ao
    // recompor sobre outro fundo.
    if (u_edge > 1e-3) {
        a = smoothstep(0.5 - u_edge * 0.5, 0.5 + u_edge * 0.5, a);
    }

    fragColor = vec4(despill(src.rgb), a * src.a);
}
