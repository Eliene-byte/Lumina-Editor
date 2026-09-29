#version 330 core
// vignette.frag - Vinheta e bordas.
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_input;
uniform float u_amount;     // 0..1
uniform float u_softness;   // 0..1
uniform vec2 u_center;
uniform float u_roundness;  // 1 = circular, 0 = retangular
uniform float u_aspect;     // largura/altura, para a vinheta nao ficar oval
uniform int u_invert;       // 1 = escurece o centro em vez das bordas

void main() {
    vec4 src = texture(u_input, v_uv);

    vec2 p = v_uv - u_center;
    p.x *= u_aspect;

    // Distancia normalizada: 0 no centro, ~1 nos cantos.
    vec2 corner = vec2(0.5 * u_aspect, 0.5);
    float maxR = length(corner);
    float d = length(p) / max(maxR, 1e-4);
    d = mix(max(abs(p.x) / max(corner.x, 1e-4), abs(p.y) / 0.5), d, u_roundness);

    float edge = smoothstep(1.0 - u_softness, 1.0, d);
    if (u_invert == 1) edge = 1.0 - edge;

    fragColor = vec4(src.rgb * (1.0 - edge * u_amount), src.a);
}
