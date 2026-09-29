// common.glsl - Funcoes compartilhadas por todos os fragmentos.
//
// Inserido via #include. Nao deve ter #version: quem inclui ja o tem.
//
// Convencao do projeto: os nos trabalham em espaco LINEAR. A conversao para
// display acontece uma unica vez, no no de saida (view_transform). Isso
// mantem blends e blurs com o comportamento correto.

const float PI = 3.14159265358979323846;

const float LUMA_R = 0.2126;
const float LUMA_G = 0.7152;
const float LUMA_B = 0.0722;

float luma(vec3 c) {
    return dot(c, vec3(LUMA_R, LUMA_G, LUMA_B));
}

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + 1e-10)), d / (q.x + 1e-10), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// --- transferencia linear --------------------------------------------------

float srgbToLinear1(float c) {
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

float linearToSrgb1(float c) {
    c = max(c, 0.0);
    return c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

vec3 srgbToLinear(vec3 c) {
    return vec3(srgbToLinear1(c.r), srgbToLinear1(c.g), srgbToLinear1(c.b));
}

vec3 linearToSrgb(vec3 c) {
    return vec3(linearToSrgb1(c.r), linearToSrgb1(c.g), linearToSrgb1(c.b));
}

// BT.1886 (gamma 2.4), Rec.709 e P3 compartilham a curva de transferencia.
vec3 gammaToDisplay(vec3 c, float gamma) {
    return pow(max(c, 0.0), vec3(1.0 / max(gamma, 0.01)));
}

vec3 displayToGamma(vec3 c, float gamma) {
    return pow(max(c, 0.0), vec3(max(gamma, 0.01)));
}

// --- ACES (aproximacao de Narkowicz) --------------------------------------

vec3 acesFitted(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// --- HLG -------------------------------------------------------------------

vec3 hlgToLinear(vec3 c) {
    const float a = 0.17883277, b = 0.28466892, cc = 0.55991073;
    return mix(c * c / 3.0, (sqrt(c * cc) + a) - b, step(b, c));
}

vec3 linearToHlg(vec3 c) {
    const float a = 0.17883277, b = 0.28466892, cc = 0.55991073;
    c = max(c, 0.0);
    return c <= 1.0 / 12.0 ? sqrt(3.0 * c) : a * log(12.0 * c - b) + cc;
}

// --- ruido -----------------------------------------------------------------

float hash11(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

vec2 hash22(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}

float hash21(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// Ruido de valor com interpolacao suave, 0..1.
float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Ruido fractal (fBm) com lacunaridade 2 e ganho 0.5, como no After Effects.
float fbm(vec2 p, int octaves, float lacunarity, float gain) {
    float sum = 0.0;
    float amp = 0.5;
    float norm = 0.0;
    for (int i = 0; i < octaves; ++i) {
        sum += vnoise(p) * amp;
        norm += amp;
        p *= lacunarity;
        amp *= gain;
    }
    return norm > 0.0 ? sum / norm : 0.0;
}

// Ruido celular (Worley) - usado por textura de vidro, pele e atmosfera.
float worley(vec2 p) {
    vec2 n = floor(p);
    vec2 f = fract(p);
    float d = 1.0;
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            vec2 g = vec2(float(i), float(j));
            vec2 o = hash22(n + g);
            d = min(d, length(g + o - f));
        }
    }
    return d;
}

// --- utilitarios -----------------------------------------------------------

float sat(float x) { return clamp(x, 0.0, 1.0); }

vec3 sat3(vec3 c) { return clamp(c, 0.0, 1.0); }

// Distancia assinada de um retangulo com bordas arredondadas. Usado por
// shapes e mascaras, que precisam de feather em pixels.
float sdRoundRect(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

float sdEllipse(vec2 p, vec2 r) {
    // Aproximacao com erro de subpixel: suficiente para feather.
    float k1 = length(p / r);
    float k2 = length(p / (r * r));
    return k1 * (k1 - 1.0) / max(k2, 1e-6);
}

// Amostra uma textura 3x3 para derivadas/anti-aliasing de bordas.
vec4 sample3x3(sampler2D tex, vec2 uv, vec2 texel) {
    vec4 c = vec4(0.0);
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            c += texture(tex, uv + vec2(float(x), float(y)) * texel);
        }
    }
    return c / 9.0;
}
