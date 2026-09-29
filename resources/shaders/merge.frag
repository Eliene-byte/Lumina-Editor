#version 330 core
// merge.frag - Modos de mesclagem em um unico shader.
//
// Um switch por uniforme e mais rapido que compilar N programas e permite
// trocar o modo com keyframes sem recompilar.
//
// Convencao: tudo entra e sai premultiplicado (rgb * a). Quem entrega a
// entrada faz a conversao, assim um merge nunca divide por zero em a = 0.
//
// Os indices seguem a ordem de core/BlendMode.h.
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_back;
uniform sampler2D u_front;
uniform int u_hasBack;
uniform int u_hasFront;
uniform int u_mode;
uniform float u_opacity;
uniform int u_applyOpacity;   // 0 = "Apply Opacity" do AE

// --- auxiliares dos modos nao separaveis -----------------------------------

vec3 setLum(vec3 c, float l) {
    float d = l - luma(c);
    vec3 v = c + vec3(d);
    float mn = min(v.r, min(v.g, v.b));
    float mx = max(v.r, max(v.g, v.b));
    if (mn < 0.0) v = vec3(l) + (v - vec3(l)) * (l / max(l - mn, 1e-5));
    if (mx > 1.0) v = vec3(l) + (v - vec3(l)) * ((1.0 - l) / max(mx - l, 1e-5));
    return v;
}

vec3 setSat(vec3 c, float s) {
    float mn = min(c.r, min(c.g, c.b));
    float mx = max(c.r, max(c.g, c.b));
    return (c - mn) * s / max(mx - mn, 1e-5);
}

// Saturacao de um pixel: max - min dos tres canais (definicao do W3C).
float satOf(vec3 c) {
    return max(c.r, max(c.g, c.b)) - min(c.r, min(c.g, c.b));
}

// --- modos separaveis (mesma curva para R, G e B) --------------------------

vec3 blendSeparable(int m, vec3 cb, vec3 cs) {
    if (m == 1)  return cb + cs;                                  // Add
    if (m == 2)  return 1.0 - (1.0 - cb) * (1.0 - cs);            // Screen
    if (m == 3)  return cb * cs;                                  // Multiply
    if (m == 4) {                                                 // Overlay
        vec3 lo = 2.0 * cb * cs;
        vec3 hi = 1.0 - 2.0 * (1.0 - cb) * (1.0 - cs);
        return mix(lo, hi, step(vec3(0.5), cb));
    }
    if (m == 5) {                                                 // SoftLight
        vec3 lo = cb - (1.0 - 2.0 * cs) * cb * (1.0 - cb);
        vec3 hi = cb + (2.0 * cs - 1.0) * (cb - cb * cb);
        return mix(lo, hi, step(vec3(0.5), cs));
    }
    if (m == 6) {                                                 // HardLight
        vec3 lo = 2.0 * cs * cb;
        vec3 hi = 1.0 - 2.0 * (1.0 - cs) * (1.0 - cb);
        return mix(lo, hi, step(vec3(0.5), cb));
    }
    if (m == 7)  return abs(cb - cs);                             // Difference
    if (m == 8)  return cb + cs - 2.0 * cb * cs;                  // Exclusion
    if (m == 11) return max(cb, cs);                              // Lighten
    if (m == 12) return min(cb, cs);                              // Darken
    if (m == 13) return mix(cb, max(cb + cs - 1.0, 0.0), step(vec3(0.5), cb));  // LinearBurn
    if (m == 14) return mix(cb, min(cb + cs, 1.0), step(vec3(0.5), cb));        // LinearDodge
    if (m == 15) return mix(cb, min(cb / max(1.0 - cs, 1e-4), 1.0), step(vec3(0.0), cs));  // ColorDodge
    if (m == 16) return mix(cb, 1.0 - min((1.0 - cb) / max(cs, 1e-4), 1.0),
                            step(vec3(1.0), cb));                 // ColorBurn
    if (m == 17) return mix(cb, step(vec3(1.0), cb + cs), step(vec3(0.0), cs));  // HardMix
    if (m == 18) return max(cb, cs);                              // Max
    if (m == 19) return min(cb, cs);                              // Min
    return cs;                                                    // Over
}

// --- modos nao separaveis ---------------------------------------------------

vec3 blendNonSeparable(int m, vec3 cb, vec3 cs) {
    if (m == 9)  return max(vec3(0.0), cb - cs);                  // Subtract
    if (m == 10) return cs > 1e-5 ? min(cb / cs, 1.0) : vec3(1.0);  // Divide
    if (m == 20) return sat3(vec3(luma(cb)) + (cs - vec3(luma(cs))));  // Luminosity
    if (m == 21) return sat3(setLum(cs, luma(cb)));                   // Saturation
    if (m == 22) return sat3(setLum(setSat(cs, satOf(cb)), luma(cb)));  // Hue
    if (m == 23) return sat3(setLum(setSat(cs, satOf(cb)), luma(cb)));  // Color
    return cs;
}

void main() {
    vec4 cb = u_hasBack == 1 ? texture(u_back, v_uv) : vec4(0.0);
    vec4 cs = u_hasFront == 1 ? texture(u_front, v_uv) : vec4(0.0);

    // "Apply Opacity" desliga: o front entra com 100% e a opacidade e
    // aplicada no resultado, como no After Effects.
    float a = cs.a * u_opacity;
    if (u_applyOpacity == 0) a = cs.a;

    vec3 blended = (u_mode <= 19) ? blendSeparable(u_mode, cb.rgb, cs.rgb)
                                  : blendNonSeparable(u_mode, cb.rgb, cs.rgb);

    // Composicao "over" com o resultado da mesclagem.
    vec3 outPremul = blended * a + cb.rgb * (1.0 - a);
    float outAlpha = a + cb.a * (1.0 - a);
    if (u_applyOpacity == 0) outAlpha *= u_opacity;

    fragColor = vec4(outPremul, outAlpha);
}
