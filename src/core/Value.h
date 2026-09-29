// Value.h - Variante tipada usada por todas as propriedades animaveis.
//
// Uma unica representacao para valores nao animaveis e animaveis e essencial
// para que o motor de expressoes, a serializacao e a UI possam tratar
// "position.x", "cor" e "nome" com o mesmo codigo.
#pragma once

#include <memory>
#include <string>
#include <variant>

#include "Types.h"

namespace lmn {

enum class ValueType : uint8_t {
    None = 0,
    Bool,
    Double,
    String,
    Color,
    Vec2,
    Vec3,
    Vec4,
};

[[nodiscard]] const char* valueTypeName(ValueType t) noexcept;
[[nodiscard]] ValueType valueTypeFromName(const std::string& name) noexcept;

// Nome curto usado no arquivo de projeto (estilo After Effects).
[[nodiscard]] const char* valueTypeShortName(ValueType t) noexcept;

class Value {
public:
    Value() = default;
    Value(bool v) : m_data(v) {}
    Value(double v) : m_data(v) {}
    Value(int v) : m_data(static_cast<double>(v)) {}
    Value(Time v) : m_data(v) {}
    Value(std::string v) : m_data(std::move(v)) {}
    Value(const char* v) : m_data(std::string(v)) {}
    Value(Color v) : m_data(v) {}
    Value(Vec2 v) : m_data(v) {}
    Value(Vec3 v) : m_data(v) {}
    Value(Vec4 v) : m_data(v) {}

    [[nodiscard]] ValueType type() const noexcept;
    [[nodiscard]] bool isValid() const noexcept { return type() != ValueType::None; }

    // Acessores tipados. Retornam um valor padrao se o tipo nao bater, para
    // que shaders em desenvolvimento nao derrubem o app.
    [[nodiscard]] double asDouble(double fallback = 0.0) const noexcept;
    [[nodiscard]] bool asBool(bool fallback = false) const noexcept;
    [[nodiscard]] std::string asString(const std::string& fallback = {}) const;
    [[nodiscard]] Color asColor(const Color& fallback = Color()) const noexcept;
    [[nodiscard]] Vec2 asVec2(const Vec2& fallback = {}) const noexcept;
    [[nodiscard]] Vec3 asVec3(const Vec3& fallback = {}) const noexcept;
    [[nodiscard]] Vec4 asVec4(const Vec4& fallback = {}) const noexcept;

    // Coercoes uteis para codigo de UI: um Double vira Vec4 igual para poder
    // alimentar sliders de cor ou-alpha sem tratamento extra.
    [[nodiscard]] double toScalar() const noexcept;
    [[nodiscard]] Vec4 toVec4() const noexcept;

    // Converte para outro tipo quando a conversao faz sentido (ex.: Vec2->Vec3).
    [[nodiscard]] bool convertTo(ValueType target);

    // Operacoes aritmeticas usadas pelo motor de expressoes e pelo blend de
    // valores. Soma/escala/mix fazem parte do contrato do Value.
    [[nodiscard]] Value add(const Value& other) const;
    [[nodiscard]] Value sub(const Value& other) const;
    [[nodiscard]] Value mul(const Value& other) const;
    [[nodiscard]] Value scale(double s) const;
    [[nodiscard]] Value mix(const Value& other, double t) const;

    // Representacao textual estavel (usada em logs, expressoes e depuracao).
    [[nodiscard]] std::string toText(int precision = 4) const;

    friend bool operator==(const Value& a, const Value& b);
    friend bool operator!=(const Value& a, const Value& b) { return !(a == b); }

    using Storage = std::variant<std::monostate, bool, double, std::string,
                                 Color, Vec2, Vec3, Vec4>;

private:
    Storage m_data;
};

// Valor padrao sensato para cada tipo - usado ao criar Properties.
[[nodiscard]] Value defaultValueFor(ValueType t);

}  // namespace lmn
