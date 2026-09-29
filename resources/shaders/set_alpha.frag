#version 330 core
// set_alpha.frag - Define o canal alpha diretamente.
//
// Usado para ajuste rapido de matte, por transicoes e por geradores que
// produzem alpha sem alterar a cor.
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_input;
uniform int u_hasInput;
uniform vec4 u_clearColor;
uniform float u_alpha;        // 0..1
uniform int u_inheritAlpha;   // 1 = multiplica pelo alpha da entrada
uniform int u_outputStraight; // 1 = devolve rgb sem multiplicar por a
uniform int u_useClearColor;  // 1 = substitui a cor, nao so o alpha

void main() {
    vec4 c = u_hasInput == 1 ? texture(u_input, v_uv) : u_clearColor;

    float a = sat(u_alpha);
    if (u_inheritAlpha == 1 && u_hasInput == 1) a *= c.a;

    if (u_outputStraight == 0) {
        // Saida premultiplicada: a cor anda junto com o alpha.
        c.rgb *= a;
    } else if (u_useClearColor == 1) {
        c.rgb = u_clearColor.rgb;
    }

    fragColor = vec4(c.rgb, a);
}
