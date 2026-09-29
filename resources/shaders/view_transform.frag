#version 330 core
// view_transform.frag - Converte do espaco linear de trabalho para o display.
//
// E o unico lugar onde a conversao de cor acontece. Todo o resto do grafo
// trabalha em linear, o que faz blends, blurs e masks se comportarem certo.
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_input;
uniform int u_display;      // 0 Standard, 1 Rec709, 2 P3, 3 HLG, 4 ACES, 5 Raw
uniform float u_exposure;    // em stops
uniform float u_gamma;
uniform vec3 u_gain;         // multiplicador por canal (white balance)
uniform vec3 u_lift;         // soma por canal (black level)
uniform float u_saturation;
uniform float u_bypass;      // 1 = mostra linear, sem conversao

// Matriz P3 (D65) -> XYZ -> sRGB, para reexibir imagens em P3 num monitor sRGB.
const mat3 P3_TO_XYZ = mat3(
    0.4865709, 0.2289746, 0.0000000,
    0.2656677, 0.6917385, 0.0451134,
    0.1982173, 0.0792869, 1.0439444
);
const mat3 XYZ_TO_REC709 = mat3(
     3.2404542, -0.9692660,  0.0556434,
    -1.5371385,  1.8760108, -0.2040259,
    -0.4985314,  0.0415560,  1.0572252
);

void main() {
    vec4 c = texture(u_input, v_uv);
    if (c.a > 0.0) c.rgb /= c.a;   // despremultiplica antes de converter

    if (u_bypass < 0.5) {
        c.rgb = c.rgb * exp2(u_exposure);
        c.rgb = c.rgb * u_gain + u_lift;

        if (u_saturation != 1.0) {
            c.rgb = mix(vec3(luma(c.rgb)), c.rgb, u_saturation);
        }

        if (u_display == 0) {
            c.rgb = linearToSrgb(c.rgb);
        } else if (u_display == 1) {
            c.rgb = gammaToDisplay(c.rgb, 2.4);
        } else if (u_display == 2) {
            vec3 xyz = P3_TO_XYZ * c.rgb;
            c.rgb = linearToSrgb(XYZ_TO_REC709 * xyz);
        } else if (u_display == 3) {
            c.rgb = linearToHlg(c.rgb);
        } else if (u_display == 4) {
            c.rgb = acesFitted(c.rgb);
            c.rgb = linearToSrgb(c.rgb);
        }
        // u_display == 5 (Raw) nao converte: e o modo dos scopes.

        c.rgb = pow(max(c.rgb, 0.0), vec3(1.0 / max(u_gamma, 0.01)));
    }

    fragColor = vec4(c.rgb, c.a);
}
