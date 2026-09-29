#version 330 core
// copy.frag - Repasse com transformacao opcional.
// Tambem usado como "pass through" de nos marcados com PassThrough.
#include "common.glsl"

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_input;
uniform int u_hasInput;
uniform vec4 u_clearColor;      // usado quando nao ha entrada

// Transformacao 2D com ancora, escala, rotacao e posicao, como no After Effects.
uniform mat3 u_inverseTransform;
uniform int u_useTransform;
uniform vec2 u_resolution;
uniform float u_flipX;
uniform float u_flipY;
uniform float u_opacity;
uniform int u_premultiply;
uniform int u_straighten;      // reverte o premultiplicado

void main() {
    vec4 c;
    if (u_hasInput == 0) {
        c = u_clearColor;
    } else {
        vec2 uv = v_uv;
        if (u_useTransform == 1) {
            // UV 0..1 com y para cima, mesma convencao do modelo de dados.
            vec2 p = (uv - 0.5) * u_resolution;
            vec2 q = (u_inverseTransform * vec3(p, 1.0)).xy;
            uv = q / u_resolution + 0.5;
        }
        uv.x = mix(uv.x, 1.0 - uv.x, u_flipX);
        uv.y = mix(uv.y, 1.0 - uv.y, u_flipY);

        // Fora da area transformada: transparente, como no AE.
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
            fragColor = vec4(0.0);
            return;
        }
        c = texture(u_input, uv);
        if (u_straighten == 1 && c.a > 0.0) c /= c.a;
    }

    if (u_premultiply == 1) c.rgb *= c.a;
    fragColor = vec4(c.rgb, c.a * u_opacity);
}
