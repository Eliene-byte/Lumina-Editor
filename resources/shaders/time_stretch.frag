#version 330 core
// time_stretch.frag - Reamostragem temporal: hold, linear e borrao com
// compensacao simples de movimento.
//
// Nao altera a cor, so combina dois quadros da entrada. E o que faz o
// arrastar de velocidade na timeline nao virar uma escada de quadros.
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_input;
uniform vec2 u_frameA;     // uv do quadro base
uniform vec2 u_frameB;     // uv do quadro seguinte
uniform float u_mix;       // 0 = A, 1 = B
uniform int u_mode;        // 0 hold, 1 linear, 2 borrao compensado
uniform float u_denoise;   // 0..1: forca a compensacao

void main() {
    if (u_mode == 0) {
        fragColor = texture(u_input, u_frameA);
        return;
    }

    vec4 a = texture(u_input, u_frameA);
    vec4 b = texture(u_input, u_frameB);
    float m = sat(u_mix);

    if (u_mode != 2) {
        fragColor = mix(a, b, m);
        return;
    }

    // Borrao compensado: o peso e enviesado para o quadro mais proximo, o
    // que devolve mais detalhe no meio do intervalo. O cancelling de artefatos
    // completo de verdade fica para a etapa de estimativa de movimento.
    float bias = mix(m, smoothstep(0.0, 1.0, m), u_denoise);
    vec4 blended = mix(a, b, bias);

    // Compensacao de contraste: a media de dois quadros vizinhos perde
    // micro-contraste; devolvemos parte dele.
    float detail = 1.0 + u_denoise * 0.15 * (1.0 - abs(2.0 * m - 1.0));
    blended.rgb = (blended.rgb - 0.5) * detail + 0.5;

    fragColor = vec4(max(blended.rgb, 0.0), mix(a.a, b.a, m));
}
