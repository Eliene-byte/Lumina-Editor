// Types.h - Tipos fundamentais do nucleo.
//
// Convencao: tempo e sempre em segundos (double). Frames so aparecem nas
// bordas (UI, export) e sao convertidos com Project::timeToFrame().
// Espaco de cor de trabalho: linear, 1.0 = branco da cena.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace lmn {

using Time = double;
using Frame = int64_t;
using NodeId = uint32_t;
using CompId = uint32_t;
using StackId = uint32_t;
using ClipId = uint32_t;
using MediaId = uint32_t;

inline constexpr NodeId kInvalidId = 0;
inline constexpr Time kEpsilon = 1e-9;

struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    constexpr Vec2() = default;
    constexpr Vec2(double xx, double yy) : x(xx), y(yy) {}

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(double s) const { return {x * s, y * s}; }
    Vec2 operator*(const Vec2& o) const { return {x * o.x, y * o.y}; }
    Vec2 operator/(double s) const { return {x / s, y / s}; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(double s) { x *= s; y *= s; return *this; }
    double length() const { return std::sqrt(x * x + y * y); }
    [[nodiscard]] bool isFinite() const { return std::isfinite(x) && std::isfinite(y); }

    static Vec2 lerp(const Vec2& a, const Vec2& b, double t) {
        return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
    }
};

inline Vec2 operator*(double s, const Vec2& v) { return {v.x * s, v.y * s}; }

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;

    constexpr Vec3() = default;
    constexpr Vec3(double xx, double yy, double zz) : x(xx), y(yy), z(zz) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    Vec3& operator+=(const Vec3& o) { *this = *this + o; return *this; }
    [[nodiscard]] bool isFinite() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }

    static Vec3 lerp(const Vec3& a, const Vec3& b, double t) {
        return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
    }
};

struct Vec4 {
    double x = 0.0, y = 0.0, z = 0.0, w = 0.0;

    constexpr Vec4() = default;
    constexpr Vec4(double xx, double yy, double zz, double ww) : x(xx), y(yy), z(zz), w(ww) {}
    explicit constexpr Vec4(const Vec3& v, double ww) : x(v.x), y(v.y), z(v.z), w(ww) {}
    constexpr Vec4(const Vec2& xy, const Vec2& zw) : x(xy.x), y(xy.y), z(zw.x), w(zw.y) {}

    Vec4 operator+(const Vec4& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    Vec4 operator-(const Vec4& o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    Vec4 operator*(double s) const { return {x * s, y * s, z * s, w * s}; }
    Vec4 operator*(const Vec4& o) const { return {x * o.x, y * o.y, z * o.z, w * o.w}; }
    Vec4& operator+=(const Vec4& o) { *this = *this + o; return *this; }
    [[nodiscard]] bool isFinite() const {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(w);
    }

    [[nodiscard]] Vec2 xy() const { return {x, y}; }

    static Vec4 lerp(const Vec4& a, const Vec4& b, double t) {
        return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
    }
};

// Cor em espaco LINEAR (nao sRGB). Alpha separado e sempre linear.
struct Color {
    double r = 0.0, g = 0.0, b = 0.0, a = 1.0;

    constexpr Color() = default;
    constexpr Color(double rr, double gg, double bb, double aa = 1.0) : r(rr), g(gg), b(bb), a(aa) {}

    [[nodiscard]] Vec3 rgb() const { return {r, g, b}; }
    [[nodiscard]] Vec4 rgba() const { return {r, g, b, a}; }
    [[nodiscard]] bool isBlack() const { return r == 0.0 && g == 0.0 && b == 0.0; }
    [[nodiscard]] bool isWhite() const { return r == 1.0 && g == 1.0 && b == 1.0; }
    [[nodiscard]] double luma() const {
        return 0.2126 * r + 0.7152 * g + 0.0722 * b;
    }

    // Amostra um gradiente 0..1 entre duas cores.
    static Color lerp(const Color& a, const Color& b, double t) {
        return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
                a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
    }

    friend bool operator==(const Color& a, const Color& b) {
        return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
    }
    friend bool operator!=(const Color& a, const Color& b) { return !(a == b); }
};

struct Size {
    int width = 1920;
    int height = 1080;
    [[nodiscard]] double aspect() const {
        return height == 0 ? 1.0 : static_cast<double>(width) / static_cast<double>(height);
    }
    [[nodiscard]] bool isValid() const { return width > 0 && height > 0; }
    friend bool operator==(const Size& a, const Size& b) {
        return a.width == b.width && a.height == b.height;
    }
    friend bool operator!=(const Size& a, const Size& b) { return !(a == b); }
};

// Retangulo no espaco da composicao, origem no centro, y para cima.
struct Rect {
    double left = 0.0, right = 0.0, bottom = 0.0, top = 0.0;

    constexpr Rect() = default;
    constexpr Rect(double l, double r, double b, double t) : left(l), right(r), bottom(b), top(t) {}

    [[nodiscard]] double width() const { return right - left; }
    [[nodiscard]] double height() const { return top - bottom; }
    [[nodiscard]] double centerX() const { return (left + right) * 0.5; }
    [[nodiscard]] double centerY() const { return (bottom + top) * 0.5; }
    [[nodiscard]] bool empty() const { return width() <= 0.0 || height() <= 0.0; }

    static Rect fromSize(const Size& s) {
        const double hw = s.width * 0.5;
        const double hh = s.height * 0.5;
        return {-hw, hw, -hh, hh};
    }

    [[nodiscard]] bool contains(double x, double y) const {
        return x >= left && x <= right && y >= bottom && y <= top;
    }
};

// Faixa de tempo inclusiva no inicio, exclusiva no fim.
struct TimeRange {
    Time in = 0.0;
    Time out = 0.0;

    constexpr TimeRange() = default;
    constexpr TimeRange(Time i, Time o) : in(i), out(o) {}

    [[nodiscard]] Time duration() const { return std::max(0.0, out - in); }
    [[nodiscard]] bool isValid() const { return out > in; }
    [[nodiscard]] bool contains(Time t) const { return t >= in && t < out; }
    [[nodiscard]] Time clamp(Time t) const { return std::clamp(t, in, out); }

    [[nodiscard]] TimeRange translated(Time delta) const { return {in + delta, out + delta}; }

    [[nodiscard]] TimeRange intersection(const TimeRange& o) const {
        const Time a = std::max(in, o.in);
        const Time b = std::min(out, o.out);
        return b > a ? TimeRange{a, b} : TimeRange{0.0, 0.0};
    }

    friend bool operator==(const TimeRange& a, const TimeRange& b) {
        return a.in == b.in && a.out == b.out;
    }
};

inline Time lerpTime(Time a, Time b, double t) { return a + (b - a) * t; }
inline Time clampTime(Time t, Time a, Time b) { return std::clamp(t, a, b); }

// Curva de suavidade usada pelas keyframes. t em 0..1 -> 0..1.
inline double smoothStepEasing(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

inline double smootherStepEasing(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// Bezier cubica com tangentes normalizadas. y0=0, y1=easeOut, y2=1-easeIn, y3=1.
inline double cubicBezierEasing(double t, double easeIn, double easeOut) {
    t = std::clamp(t, 0.0, 1.0);
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    const double p1t = std::max(1e-3, easeOut);
    const double p2t = std::min(1.0 - 1e-3, 1.0 - easeIn);

    // Aproximacao por bissecao: barata e estavel numericamente.
    double lo = 0.0, hi = 1.0, u = t;
    for (int i = 0; i < 24; ++i) {
        const double mt = 1.0 - u;
        const double x = 3.0 * mt * mt * u * p1t + 3.0 * mt * u * u * p2t + u * u * u;
        if (x < t) { lo = u; } else { hi = u; }
        u = 0.5 * (lo + hi);
    }
    const double mt = 1.0 - u;
    return 3.0 * mt * mt * u * easeOut + 3.0 * mt * u * u * (1.0 - easeIn) + u * u * u;
}

}  // namespace lmn
