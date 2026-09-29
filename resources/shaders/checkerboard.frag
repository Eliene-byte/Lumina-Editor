#version 330 core
// checkerboard.frag - Fundo xadrez da area transparente no visualizador.
//
// Nao faz parte do render: e desenhado por baixo da composicao para o usuario
// enxergar o que e transparente. Nunca entra no arquivo exportado.
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform vec2 u_texel;
uniform vec4 u_colorA;
uniform vec4 u_colorB;
uniform int u_squares;   // tamanho em pixels

void main() {
    ivec2 p = ivec2(v_uv / u_texel);
    int s = max(u_squares, 2);
    int parity = (p.x / s + p.y / s) & 1;
    fragColor = parity == 0 ? u_colorA : u_colorB;
}
