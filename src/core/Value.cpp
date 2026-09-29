#include "Value.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace lmn {
namespace {

template <typename T>
std::string formatNumber(const T& v, int precision) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*g", precision, static_cast<double>(v));
    return std::string(buf);
}

std::string formatVec(int n, const double* c, int precision) {
    std::string out = "[";
    for (int i = 0; i < n; ++i) {
        if (i) out += ", ";
        out += formatNumber(c[i], precision);
    }
    out += "]";
    return out;
}

}  // namespace

const char* valueTypeName(ValueType t) noexcept {
    switch (t) {
        case ValueType::None:   return "None";
        case ValueType::Bool:   return "Bool";
        case ValueType::Double: return "Double";
        case ValueType::String: return "String";
        case ValueType::Color:  return "Color";
        case ValueType::Vec2:   return "Vec2";
        case ValueType::Vec3:   return "Vec3";
        case ValueType::Vec4:   return "Vec4";
    }
    return "None";
}

const char* valueTypeShortName(ValueType t) noexcept {
    switch (t) {
        case ValueType::None:   return "none";
        case ValueType::Bool:   return "bool";
        case ValueType::Double: return "double";
        case ValueType::String: return "string";
        case ValueType::Color:  return "color";
        case ValueType::Vec2:   return "vec2";
        case ValueType::Vec3:   return "vec3";
        case ValueType::Vec4:   return "vec4";
    }
    return "none";
}

ValueType valueTypeFromName(const std::string& name) noexcept {
    if (name == "Bool")   return ValueType::Bool;
    if (name == "Double") return ValueType::Double;
    if (name == "String") return ValueType::String;
    if (name == "Color")  return ValueType::Color;
    if (name == "Vec2")   return ValueType::Vec2;
    if (name == "Vec3")   return ValueType::Vec3;
    if (name == "Vec4")   return ValueType::Vec4;
    return ValueType::None;
}

ValueType Value::type() const noexcept {
    switch (m_data.index()) {
        case 1:  return ValueType::Bool;
        case 2:  return ValueType::Double;
        case 3:  return ValueType::String;
        case 4:  return ValueType::Color;
        case 5:  return ValueType::Vec2;
        case 6:  return ValueType::Vec3;
        case 7:  return ValueType::Vec4;
        default: return ValueType::None;
    }
}

double Value::asDouble(double fallback) const noexcept {
    if (const double* v = std::get_if<double>(&m_data)) return *v;
    if (const bool* v = std::get_if<bool>(&m_data)) return *v ? 1.0 : 0.0;
    if (const Vec2* v = std::get_if<Vec2>(&m_data)) return v->x;
    if (const Vec3* v = std::get_if<Vec3>(&m_data)) return v->x;
    if (const Vec4* v = std::get_if<Vec4>(&m_data)) return v->x;
    if (const Color* v = std::get_if<Color>(&m_data)) return v->r;
    return fallback;
}

bool Value::asBool(bool fallback) const noexcept {
    if (const bool* v = std::get_if<bool>(&m_data)) return *v;
    if (const double* v = std::get_if<double>(&m_data)) return *v != 0.0;
    return fallback;
}

std::string Value::asString(const std::string& fallback) const {
    if (const std::string* v = std::get_if<std::string>(&m_data)) return *v;
    return fallback;
}

Color Value::asColor(const Color& fallback) const noexcept {
    if (const Color* v = std::get_if<Color>(&m_data)) return *v;
    if (const Vec4* v = std::get_if<Vec4>(&m_data)) return Color{v->x, v->y, v->z, v->w};
    if (const Vec3* v = std::get_if<Vec3>(&m_data)) return Color{v->x, v->y, v->z, 1.0};
    if (const double* v = std::get_if<double>(&m_data)) return Color{*v, *v, *v, 1.0};
    return fallback;
}

Vec2 Value::asVec2(const Vec2& fallback) const noexcept {
    if (const Vec2* v = std::get_if<Vec2>(&m_data)) return *v;
    if (const Vec3* v = std::get_if<Vec3>(&m_data)) return {v->x, v->y};
    if (const Vec4* v = std::get_if<Vec4>(&m_data)) return {v->x, v->y};
    if (const double* v = std::get_if<double>(&m_data)) return {*v, *v};
    if (const bool* v = std::get_if<bool>(&m_data)) return {*v ? 1.0 : 0.0, *v ? 1.0 : 0.0};
    return fallback;
}

Vec3 Value::asVec3(const Vec3& fallback) const noexcept {
    if (const Vec3* v = std::get_if<Vec3>(&m_data)) return *v;
    if (const Vec2* v = std::get_if<Vec2>(&m_data)) return {v->x, v->y, 0.0};
    if (const Vec4* v = std::get_if<Vec4>(&m_data)) return {v->x, v->y, v->z};
    if (const Color* v = std::get_if<Color>(&m_data)) return {v->r, v->g, v->b};
    if (const double* v = std::get_if<double>(&m_data)) return {*v, *v, *v};
    return fallback;
}

Vec4 Value::asVec4(const Vec4& fallback) const noexcept {
    if (const Vec4* v = std::get_if<Vec4>(&m_data)) return *v;
    if (const Color* v = std::get_if<Color>(&m_data)) return v->rgba();
    if (const Vec3* v = std::get_if<Vec3>(&m_data)) return {v->x, v->y, v->z, 1.0};
    if (const Vec2* v = std::get_if<Vec2>(&m_data)) return {v->x, v->y, 0.0, 1.0};
    if (const double* v = std::get_if<double>(&m_data)) return {*v, *v, *v, 1.0};
    if (const bool* v = std::get_if<bool>(&m_data)) {
        const double d = *v ? 1.0 : 0.0;
        return {d, d, d, d};
    }
    return fallback;
}

double Value::toScalar() const noexcept {
    if (type() == ValueType::String) return 0.0;
    return asDouble(0.0);
}

Vec4 Value::toVec4() const noexcept {
    if (type() == ValueType::String) return {};
    return asVec4({});
}

bool Value::convertTo(ValueType target) {
    if (target == type()) return true;
    if (target == ValueType::None) return false;
    // String nao converte para numero: texto e texto.
    if (type() == ValueType::String) return false;

    switch (target) {
        case ValueType::Bool:   m_data = asBool(); return true;
        case ValueType::Double: m_data = asDouble(); return true;
        case ValueType::Color:  m_data = asColor(); return true;
        case ValueType::Vec2:   m_data = asVec2();  return true;
        case ValueType::Vec3:   m_data = asVec3();  return true;
        case ValueType::Vec4:   m_data = asVec4();  return true;
        default: return false;
    }
}

Value Value::add(const Value& other) const {
    switch (type()) {
        case ValueType::Double: return Value(asDouble() + other.toScalar());
        case ValueType::Bool:   return Value(asBool() != other.asBool());
        case ValueType::Vec2: {
            const Vec2 a = asVec2();
            if (other.type() == ValueType::Double) return Value(a + Vec2{other.toScalar(), other.toScalar()});
            const Vec2 b = other.asVec2();
            return Value(a + b);
        }
        case ValueType::Vec3: {
            const Vec3 a = asVec3();
            if (other.type() == ValueType::Double) return Value(a * (1.0 + other.toScalar()));
            return Value(a + other.asVec3());
        }
        case ValueType::Vec4: {
            const Vec4 a = asVec4();
            if (other.type() == ValueType::Double) return Value(a * (1.0 + other.toScalar()));
            return Value(a + other.asVec4());
        }
        case ValueType::Color: {
            const Color a = asColor();
            const Color b = other.asColor();
            return Value(Color{a.r + b.r, a.g + b.g, a.b + b.b, a.a + b.a});
        }
        case ValueType::String: {
            const std::string b = other.type() == ValueType::String ? other.asString() : other.toText();
            return Value(asString() + b);
        }
        default: return *this;
    }
}

Value Value::sub(const Value& other) const {
    switch (type()) {
        case ValueType::Double: return Value(asDouble() - other.toScalar());
        case ValueType::Vec2: {
            const Vec2 a = asVec2();
            if (other.type() == ValueType::Double) return Value(a - Vec2{other.toScalar(), other.toScalar()});
            return Value(a - other.asVec2());
        }
        case ValueType::Vec3: {
            const Vec3 a = asVec3();
            if (other.type() == ValueType::Double) return Value(a * (1.0 - other.toScalar()));
            return Value(a - other.asVec3());
        }
        case ValueType::Vec4: {
            const Vec4 a = asVec4();
            if (other.type() == ValueType::Double) return Value(a * (1.0 - other.toScalar()));
            return Value(a - other.asVec4());
        }
        case ValueType::Color: {
            const Color a = asColor();
            const Color b = other.asColor();
            return Value(Color{a.r - b.r, a.g - b.g, a.b - b.b, a.a - b.a});
        }
        case ValueType::String: {
            // Subtruir texto nao faz sentido; devolve o proprio texto.
            return *this;
        }
        default: return *this;
    }
}

Value Value::mul(const Value& other) const {
    switch (type()) {
        case ValueType::Double: return Value(asDouble() * other.toScalar());
        case ValueType::Bool:   return Value(asBool() && other.asBool());
        case ValueType::Vec2: {
            const Vec2 a = asVec2();
            if (other.type() == ValueType::Double) return Value(a * other.toScalar());
            return Value(a * other.asVec2());
        }
        case ValueType::Vec3: {
            const Vec3 a = asVec3();
            if (other.type() == ValueType::Double) return Value(a * other.toScalar());
            return Value(a * other.asVec3());
        }
        case ValueType::Vec4: {
            const Vec4 a = asVec4();
            if (other.type() == ValueType::Double) return Value(a * other.toScalar());
            return Value(a * other.asVec4());
        }
        case ValueType::Color: {
            const Color a = asColor();
            const Color b = other.asColor();
            return Value(Color{a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a});
        }
        case ValueType::String: {
            // Em After Effects, texto * numero = repeticao. Suportamos isso.
            const int n = static_cast<int>(other.toScalar());
            if (n <= 0) return Value(std::string());
            std::string out;
            out.reserve(asString().size() * static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) out += asString();
            return Value(out);
        }
        default: return *this;
    }
}

Value Value::scale(double s) const {
    switch (type()) {
        case ValueType::Double: return Value(asDouble() * s);
        case ValueType::Vec2:   return Value(asVec2() * s);
        case ValueType::Vec3:   return Value(asVec3() * s);
        case ValueType::Vec4:   return Value(asVec4() * s);
        case ValueType::Color: {
            const Color c = asColor();
            return Value(Color{c.r * s, c.g * s, c.b * s, c.a * s});
        }
        default: return *this;
    }
}

Value Value::mix(const Value& other, double t) const {
    switch (type()) {
        case ValueType::Double: {
            const double a = asDouble(), b = other.toScalar();
            return Value(a + (b - a) * t);
        }
        case ValueType::Vec2:   return Value(Vec2::lerp(asVec2(), other.asVec2(), t));
        case ValueType::Vec3:   return Value(Vec3::lerp(asVec3(), other.asVec3(), t));
        case ValueType::Vec4:   return Value(Vec4::lerp(asVec4(), other.asVec4(), t));
        case ValueType::Color:  return Value(Color::lerp(asColor(), other.asColor(), t));
        default: return t < 0.5 ? *this : other;
    }
}

std::string Value::toText(int precision) const {
    switch (type()) {
        case ValueType::Bool:   return asBool() ? "true" : "false";
        case ValueType::Double: return formatNumber(asDouble(), precision);
        case ValueType::String: return asString();
        case ValueType::Color: {
            const Color c = asColor();
            return formatVec(4, &c.r, precision);
        }
        case ValueType::Vec2: {
            const Vec2 v = asVec2();
            return formatVec(2, &v.x, precision);
        }
        case ValueType::Vec3: {
            const Vec3 v = asVec3();
            return formatVec(3, &v.x, precision);
        }
        case ValueType::Vec4: {
            const Vec4 v = asVec4();
            return formatVec(4, &v.x, precision);
        }
        default: return "null";
    }
}

bool operator==(const Value& a, const Value& b) {
    if (a.type() != b.type()) return false;
    switch (a.type()) {
        case ValueType::None:   return true;
        case ValueType::Bool:   return a.asBool() == b.asBool();
        case ValueType::Double: return a.asDouble() == b.asDouble();
        case ValueType::String: return a.asString() == b.asString();
        case ValueType::Color:  return a.asColor() == b.asColor();
        case ValueType::Vec2:   return a.asVec2() == b.asVec2();
        case ValueType::Vec3:   return a.asVec3() == b.asVec3();
        case ValueType::Vec4:   return a.asVec4() == b.asVec4();
    }
    return false;
}

Value defaultValueFor(ValueType t) {
    switch (t) {
        case ValueType::Bool:   return Value(false);
        case ValueType::Double: return Value(0.0);
        case ValueType::String: return Value(std::string());
        case ValueType::Color:  return Value(Color{0.0, 0.0, 0.0, 1.0});
        case ValueType::Vec2:   return Value(Vec2{0.0, 0.0});
        case ValueType::Vec3:   return Value(Vec3{0.0, 0.0, 0.0});
        case ValueType::Vec4:   return Value(Vec4{0.0, 0.0, 0.0, 1.0});
        default: return Value{};
    }
}

}  // namespace lmn
