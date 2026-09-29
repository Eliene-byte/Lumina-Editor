#version 330 core
// solid.frag - Cor solida, base de quase todo grafo.
// Com type = 0 a cor e o preenchimento; 1..5 sao gradientes usados pelo
// gerador de fundos sem depender de imagem.
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform vec4 u_color;       // RGBA linear
uniform int u_gradient;     // 0 solido, 1 linear, 2 radial, 3 angular, 4 conico, 5 diamond
uniform vec2 u_start;
uniform vec2 u_end;
uniform vec4 u_color2;
uniform float u_repeat;     // repete o gradiente N vezes
uniform float u_dither;     // 0..1, quebra banding em gradientes suaves

float gradientMask(vec2 uv, int type, vec2 a, vec2 b, float repeat) {
    vec2 p = uv;
    if (repeat > 0.0) p = fract(p * max(repeat, 0.0001));

    if (type == 1) {                       // linear
        return clamp(dot(p - a, b - a) / max(dot(b - a, b - a), 1e-6), 0.0, 1.0);
    }
    if (type == 2) {                       // radial
        return clamp(length(p - a) / max(length(b - a), 1e-6), 0.0, 1.0);
    }
    if (type == 3) {                       // angular
        vec2 d = p - a;
        float ang = atan(d.y, d.x);
        ang = fract(ang / (2.0 * PI) + 0.5);
        return mix(ang, sat(length(d) / max(length(b - a), 1e-6)), 0.0);
    }
    if (type == 4) {                       // conico
        vec2 d = normalize(p - a + 1e-6);
        return sat(dot(d, normalize(b - a + 1e-6)) * 0.5 + 0.5);
    }
    if (type == 5) {                       // diamond
        vec2 d = abs(p - a) / max(abs(b - a), 1e-6);
        return clamp(d.x + d.y, 0.0, 1.0);
    }
    return 0.0;
}

void main() {
    vec4 c = u_color;
    if (u_gradient != 0) {
        float t = gradientMask(v_uv, u_gradient, u_start, u_end, u_repeat);
        c = mix(u_color, u_color2, t);
    }

    // Dither triangular de +/- 0.5/255: barato e elimina o banding que aparece
    // em fundos de cor suave em videos de 8 bits.
    if (u_dither > 0.0) {
        float n = hash21(gl_FragCoord.xy) - hash21(gl_FragCoord.xy + 17.3);
        c.rgb += n * u_dither * (0.5 / 255.0);
    }
    fragColor = c;
}
