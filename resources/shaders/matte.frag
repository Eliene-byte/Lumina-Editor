#version 330 core
// matte.frag - Opera o matte de um no: extrai, inverte, dilata, corroe e
// combina mattes (uniao, interseccao, diferenca, maximo).
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_input;
uniform sampler2D u_matte;   // entrada 1 quando ha matte externo
uniform int u_hasMatte;
uniform int u_matteSource;   // 0 alpha, 1 luminance, 2 canal R, 3 canal G,
                             // 4 canal B, 5 canal A
uniform int u_operation;     // 0 add, 1 subtract, 2 intersect, 3 union,
                             // 4 not, 5 max
uniform int u_invert;
uniform float u_grow;        // -1..1: positiva dilata, negativa corroe
uniform float u_feather;     // em pixels
uniform vec2 u_texel;

float matteAt(vec2 uv) {
    if (u_hasMatte == 0) {
        // Sem matte externo, a propria entrada e o matte.
        return texture(u_input, uv).a;
    }
    vec4 c = texture(u_matte, clamp(uv, 0.0, 1.0));
    if (u_matteSource == 0) return c.a;
    if (u_matteSource == 1) return luma(c.rgb);
    if (u_matteSource == 2) return c.r;
    if (u_matteSource == 3) return c.g;
    if (u_matteSource == 4) return c.b;
    return c.a;
}

void main() {
    vec4 src = texture(u_input, v_uv);
    float m = matteAt(v_uv);
    if (u_invert == 1) m = 1.0 - m;

    if (u_feather > 0.01 || abs(u_grow) > 1e-3) {
        // Dilatar/corromer e uma convolucao: grow positivo soma o entorno.
        float r = max(u_feather, 0.0);
        float sum = 0.0;
        for (int y = -2; y <= 2; ++y) {
            for (int x = -2; x <= 2; ++x) {
                vec2 d = u_texel * vec2(float(x), float(y)) * (r + 1.0);
                sum += matteAt(v_uv + d);
            }
        }
        m = sat(sum / 25.0 + u_grow);
    }

    float a;
    if (u_operation == 1)      a = 1.0 - m;                        // subtract
    else if (u_operation == 2) a = src.a * m;                      // intersect
    else if (u_operation == 3) a = src.a + m - src.a * m;          // union
    else if (u_operation == 4) a = 1.0 - src.a * m;                // not
    else if (u_operation == 5) a = max(src.a, m);                  // max
    else                       a = src.a * m;                      // add

    fragColor = vec4(src.rgb, sat(a));
}
