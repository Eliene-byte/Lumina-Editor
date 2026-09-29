#version 330 core
// shape.frag - Formas geometricas com preenchimento, contorno e feather.
//
// Rectangulo, elipse, triangulo e estrela. O contorno e separado do
// preenchimento para permitir a mascara de uma camada (preenchimento +
// contorno invertido).
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform vec4 u_fillColor;
uniform vec4 u_strokeColor;
uniform float u_strokeWidth;
uniform float u_feather;     // suavidade da borda, em pixels
uniform float u_roundness;   // 0 = retangulo, 1 = pill
uniform vec2 u_center;       // em UV, 0.5,0.5 = centro
uniform vec2 u_halfSize;     // em UV
uniform int u_shape;         // 0 rect, 1 ellipse, 2 triangle, 3 star, 4 polygon
uniform int u_sides;
uniform float u_innerRadius; // estrela/poligono, 0..1
uniform float u_rotation;    // graus
uniform int u_fillEnabled;
uniform int u_strokeEnabled;
uniform vec2 u_resolution;

// Poligono regular (e estrela) como intersecao de meia-planos: para cada
// lado, dist ao plano da face. Dentro do poligono e exato; fora e uma
// aproximacao boa o bastante para mascara com feather.
float sdRegular(vec2 p, float radius, int sides, float inner, float star) {
    int n = clamp(sides, 3, 16);
    float rot = u_rotation * PI / 180.0;
    float apothem = radius * cos(PI / float(n));
    float d = -1e9;
    for (int k = 0; k < n; ++k) {
        float ang = rot + (float(k) + 0.5) * (2.0 * PI / float(n));
        vec2 normal = vec2(cos(ang), sin(ang));
        // Na estrela, as pontas ficam mais longe que as valetas.
        float r = (star > 0.5 && (k % 2) == 1) ? radius : apothem * mix(1.0, inner, star);
        d = max(d, dot(p, normal) - r);
    }
    return d;
}

void main() {
    vec2 px = (v_uv - u_center) * u_resolution;
    vec2 halfPx = u_halfSize * u_resolution;

    float d;
    if (u_shape == 1) {
        d = sdEllipse(px, halfPx);
    } else if (u_shape == 2) {
        // Triangulo equilateral via intersecao de tres metades de plano.
        float a = u_rotation * PI / 180.0;
        vec2 p = vec2(px.x * cos(a) - px.y * sin(a),
                      px.x * sin(a) + px.y * cos(a));
        vec2 n1 = vec2(0.0, 1.0);
        vec2 n2 = vec2(-0.866, -0.5);
        vec2 n3 = vec2(0.866, -0.5);
        float R = min(halfPx.x, halfPx.y) * 1.15;
        d = -1.0;
        d = max(d, dot(p, n1) - R * 0.866);
        d = max(d, dot(p, n2) - R * 0.866);
        d = max(d, dot(p, n3) - R * 0.866);
    } else if (u_shape == 3 || u_shape == 4) {
        d = sdRegular(px, min(halfPx.x, halfPx.y), max(u_sides, 3),
                      u_innerRadius, u_shape == 3 ? 1.0 : 0.0);
    } else {
        float radius = min(halfPx.x, halfPx.y) * u_roundness;
        d = sdRoundRect(px, halfPx, radius);
    }

    // feather = 0 mantem a borda dura (1 pixel de antialiasing).
    float aa = max(fwidth(d), 1e-4);
    float soft = max(u_feather, aa);

    float fillMask = 1.0 - smoothstep(-soft, soft, d);
    float strokeMask = 0.0;
    if (u_strokeEnabled == 1) {
        float outer = d - u_strokeWidth;
        strokeMask = smoothstep(-soft, soft, outer) - fillMask;
    }

    vec3 c = vec3(0.0);
    float a = 0.0;
    if (u_fillEnabled == 1) {
        c = u_fillColor.rgb * fillMask;
        a = u_fillColor.a * fillMask;
    }
    if (u_strokeEnabled == 1) {
        c = c * (1.0 - strokeMask) + u_strokeColor.rgb * strokeMask;
        a = a * (1.0 - strokeMask) + u_strokeColor.a * strokeMask;
    }

    fragColor = vec4(c, a);
}
