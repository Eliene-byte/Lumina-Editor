#version 330 core
// curves.frag - Curvas de correcao por canal e combinadas.
//
// Cada canal recebe ate 8 pontos de controle, definidos por pares (x, y) em
// uniforms. A interpolacao e um spline monotonico (monotone cubic) para a
// curva nunca dar "barriga" entre dois pontos - e o que diferencia uma curva
// usavel de uma que estoura nas sombras.
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_input;

#define MAX_POINTS 8

uniform int u_masterCount;
uniform float u_master[MAX_POINTS * 2];
uniform int u_redCount;
uniform float u_red[MAX_POINTS * 2];
uniform int u_greenCount;
uniform float u_green[MAX_POINTS * 2];
uniform int u_blueCount;
uniform float u_blue[MAX_POINTS * 2];

// Interpolacao linear por partes entre os pontos de controle.
//
// Linear, e nao spline, de proposito: nunca gera overshoot entre dois pontos
// (barriga nas sombras), que e o defeito que mais incomoda quem corrigir cor.
// O spline suave entra depois, com os pontos extras por cima.
float evalCurve(const float* pts, int count, float x) {
    if (count < 2) return clamp(x, 0.0, 1.0);
    x = clamp(x, 0.0, 1.0);

    int i = 0;
    for (int k = 0; k < count - 1; ++k) {
        if (x >= pts[k * 2]) i = k;
    }
    if (i >= count - 1) i = count - 2;

    float x0 = pts[i * 2],     y0 = pts[i * 2 + 1];
    float x1 = pts[i * 2 + 2], y1 = pts[i * 2 + 3];
    if (x1 - x0 < 1e-6) return clamp(y1, 0.0, 1.0);

    return clamp(mix(y0, y1, (x - x0) / (x1 - x0)), 0.0, 1.0);
}

void main() {
    vec4 src = texture(u_input, v_uv);
    vec3 c = src.rgb;

    if (u_redCount >= 2)   c.r = evalCurve(u_red, u_redCount, c.r);
    if (u_greenCount >= 2) c.g = evalCurve(u_green, u_greenCount, c.g);
    if (u_blueCount >= 2)  c.b = evalCurve(u_blue, u_blueCount, c.b);
    if (u_masterCount >= 2) {
        c.r = evalCurve(u_master, u_masterCount, c.r);
        c.g = evalCurve(u_master, u_masterCount, c.g);
        c.b = evalCurve(u_master, u_masterCount, c.b);
    }

    fragColor = vec4(c, src.a);
}
