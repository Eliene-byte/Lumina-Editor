#version 330 core
// hue_saturation.frag - Matiz, saturacao, luminosidade e colorize, no estilo
// do filtro Couleur do After Effects.
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_input;
uniform float u_hue;         // -180..180
uniform float u_saturation;  // -100..100
uniform float u_lightness;   // -100..100
uniform float u_colorize;    // 0..1
uniform float u_colorizeHue; // 0..360
uniform float u_colorizeSat; // 0..100
uniform int u_useAdobe;      // 1 = curva do AE, 0 = matematica direta

void main() {
    vec4 src = texture(u_input, v_uv);
    vec3 hsv = rgb2hsv(clamp(src.rgb, 0.0, 1.0));

    if (u_useAdobe == 1) {
        // O AE concatena as quatro operacoes em uma unica tabela, o que produz
        // um resultado diferente (e mais previsivel) em cores extremas.
        hsv.x = fract(hsv.x + u_hue / 360.0);
        float s = hsv.y * (1.0 + u_saturation / 100.0);
        hsv.y = s < 0.0 ? 0.0 : (s > 1.0 ? 1.0 : s);
        hsv.z = u_lightness >= 0.0
                    ? hsv.z + (1.0 - hsv.z) * (u_lightness / 100.0)
                    : hsv.z * (1.0 + u_lightness / 100.0);
    } else {
        hsv.x = fract(hsv.x + u_hue / 360.0);
        hsv.y = sat(hsv.y + u_saturation / 100.0);
        hsv.z = sat(hsv.z + u_lightness / 200.0);
    }

    vec3 c = hsv2rgb(hsv);

    if (u_colorize > 0.0) {
        vec3 tint = hsv2rgb(vec3(u_colorizeHue / 360.0,
                                 sat(u_colorizeSat / 100.0),
                                 1.0));
        c = mix(c, tint * luma(src.rgb), u_colorize);
    }

    fragColor = vec4(c, src.a);
}
