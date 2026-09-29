#version 330 core
// displace.frag - Deslocamento por mapa (e turbulencia).
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_input;
uniform sampler2D u_map;      // null quando o deslocamento e por turbulencia
uniform int u_hasMap;
uniform vec2 u_mapScale;      // repeat do mapa
uniform vec2 u_mapOffset;
uniform int u_mapChannel;     // 0 R, 1 G, 2 B, 3 A, 4 luminancia
uniform float u_amountX;
uniform float u_amountY;
uniform float u_centerBias;   // 0 = desloca igual em todo lugar, 1 = centro parado
uniform int u_edgeMode;       // 0 transparent, 1 wrap, 2 clamp, 3 duplicate
uniform float u_invert;       // 1 = inverte o mapa
uniform vec2 u_resolution;

// Turbulencia: mesmo ruido do gerador de textura, aplicada direto na direcao.
vec2 turbulence(vec2 uv, float scale, float octaves) {
    float sumX = 0.0, sumY = 0.0, amp = 0.5, norm = 0.0, s = scale;
    for (int i = 0; i < 4; ++i) {
        if (float(i) >= octaves) break;
        sumX += (vnoise(uv * s) - 0.5) * amp;
        sumY += (vnoise(uv * s + 31.7) - 0.5) * amp;
        norm += amp;
        s *= 2.0;
        amp *= 0.5;
    }
    return norm > 0.0 ? vec2(sumX, sumY) / norm : vec2(0.0);
}

float mapValue(vec2 uv) {
    if (u_hasMap == 0) {
        vec2 t = turbulence(uv, 8.0, 3.0);
        return 0.5 + 0.5 * t.x;
    }
    vec2 m = fract(uv * u_mapScale + u_mapOffset);
    vec4 c = texture(u_map, m);
    float v;
    if (u_mapChannel == 4) v = luma(c.rgb);
    else v = c[u_mapChannel];
    if (u_invert > 0.5) v = 1.0 - v;
    return v;
}

void main() {
    vec2 uv = v_uv;
    // Amostra o mapa no pixel original (antes do deslocamento), como no AE.
    float v = mapValue(uv) - 0.5;

    vec2 offset = vec2(u_amountX, u_amountY) * v * 2.0;

    if (u_centerBias > 0.0) {
        // Quanto mais perto do centro, menos deslocamento: e o modo "distort".
        float r = length(uv - 0.5) * 2.0;
        offset *= mix(1.0, r, u_centerBias);
    }

    vec2 suv = uv + offset;

    if (u_edgeMode == 0) {
        if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) {
            fragColor = vec4(0.0);
            return;
        }
    } else if (u_edgeMode == 1) {
        suv = fract(suv);
    } else if (u_edgeMode == 2) {
        suv = clamp(suv, 0.0, 1.0);
    } else {
        // duplicate: espelha nas bordas
        suv = abs(fract(suv * 0.5) * 2.0 - 1.0);
    }

    fragColor = texture(u_input, suv);
}
