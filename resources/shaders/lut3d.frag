#version 330 core
// lut3d.frag - Aplicacao de LUT 3D (.cube).
//
// A LUT entra como textura 2D: cada fatia do cubo vira um tile, e os tiles
// sao distribuídos numa grade. E assim que o .cube vira textura na importacao
// (ver media/Lut3D.cpp), o que evita enviar um volume 3D para a GPU.
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_input;
uniform sampler2D u_lut;
uniform int u_hasLut;
uniform float u_size;         // resolucao do cubo (16, 32, 64)
uniform float u_tilesPerRow;
uniform float u_intensity;
uniform int u_domain;         // 0 log, 1 linear, 2 video

vec3 sampleLut(vec3 c) {
    // Fatia = canal azul do valor procurado.
    float b = sat(c.b) * u_size;
    float slice = floor(b);
    float bx = fract(b);

    float col = mod(slice, u_tilesPerRow);
    float row = floor(slice / u_tilesPerRow);

    // Dentes de serra: o ultimo pixel de cada tile e a borda do proximo.
    vec2 inTile = vec2(c.r, mix(bx, bx - 1.0, step(0.999999, bx)));
    vec2 uv = (vec2(col, row) + inTile) / u_tilesPerRow;

    return texture(u_lut, clamp(uv, 0.0, 1.0)).rgb;
}

void main() {
    vec4 src = texture(u_input, v_uv);
    vec3 c = src.rgb;

    if (u_hasLut == 1) {
        vec3 lookup = c;
        if (u_domain == 0) {
            // Dominio log: leva para linear antes de procurar na LUT.
            lookup = pow(max(c, 1e-5), vec3(2.2));
        }
        vec3 mapped = sampleLut(sat3(lookup));
        if (u_domain == 0) mapped = pow(max(mapped, 0.0), vec3(1.0 / 2.2));
        c = mix(c, mapped, sat(u_intensity));
    }

    fragColor = vec4(c, src.a);
}
