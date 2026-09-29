#include "Property.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace lmn {
namespace {

std::shared_ptr<ExpressionBackend>& backendSlot() {
    static std::shared_ptr<ExpressionBackend> s_backend;
    return s_backend;
}

constexpr uint32_t kNoIndex = std::numeric_limits<uint32_t>::max();

}  // namespace

const char* interpName(Interp i) noexcept {
    switch (i) {
        case Interp::Linear: return "linear";
        case Interp::Bezier: return "bezier";
        case Interp::Hold:    return "hold";
    }
    return "bezier";
}

Interp interpFromName(const std::string& s) noexcept {
    if (s == "linear") return Interp::Linear;
    if (s == "hold")   return Interp::Hold;
    return Interp::Bezier;
}

void setExpressionBackend(std::shared_ptr<ExpressionBackend> backend) {
    backendSlot() = std::move(backend);
}

std::shared_ptr<ExpressionBackend> expressionBackend() {
    return backendSlot();
}

Property::Property(Value defaultValue)
    : m_base(std::move(defaultValue)), m_type(m_base.type()) {}

Property::Property(Value defaultValue, PropertyUi ui)
    : m_base(std::move(defaultValue)), m_type(m_base.type()), m_ui(ui) {}

void Property::setBaseValue(const Value& v) {
    if (v.type() != m_type && m_type != ValueType::None) {
        Value converted = v;
        if (!converted.convertTo(m_type)) return;
        m_base = std::move(converted);
    } else {
        m_base = v;
        if (m_type == ValueType::None) m_type = m_base.type();
    }
    m_dirty = true;
}

void Property::setExpression(std::string e) {
    m_expression = std::move(e);
    m_dirty = true;
}

void Property::clampToRange(Value& v) const {
    if (!m_clamps || m_max <= m_min) return;
    switch (v.type()) {
        case ValueType::Double: {
            double d = std::clamp(v.asDouble(), m_min, m_max);
            v = Value(d);
            break;
        }
        case ValueType::Vec2: {
            Vec2 p = v.asVec2();
            p.x = std::clamp(p.x, m_min, m_max);
            p.y = std::clamp(p.y, m_min, m_max);
            v = Value(p);
            break;
        }
        case ValueType::Color: {
            Color c = v.asColor();
            c.r = std::clamp(c.r, m_min, m_max);
            c.g = std::clamp(c.g, m_min, m_max);
            c.b = std::clamp(c.b, m_min, m_max);
            c.a = std::clamp(c.a, m_min, m_max);
            v = Value(c);
            break;
        }
        case ValueType::Vec4: {
            Vec4 p = v.asVec4();
            p.x = std::clamp(p.x, m_min, m_max);
            p.y = std::clamp(p.y, m_min, m_max);
            p.z = std::clamp(p.z, m_min, m_max);
            p.w = std::clamp(p.w, m_min, m_max);
            v = Value(p);
            break;
        }
        default: break;
    }
}

Value Property::sampleCurve(Time t) const {
    if (m_keys.empty()) return m_base;
    if (m_keys.size() == 1 || t <= m_keys.front().time) return m_keys.front().value;
    if (t >= m_keys.back().time) return m_keys.back().value;

    // Busca binaria do segmento.
    size_t hi = m_keys.size() - 1;
    size_t lo = 0;
    while (hi - lo > 1) {
        const size_t mid = (lo + hi) / 2;
        if (m_keys[mid].time <= t) lo = mid; else hi = mid;
    }
    return evaluateSegment(t, static_cast<uint32_t>(lo));
}

Value Property::evaluateSegment(Time t, uint32_t index) const {
    if (m_keys.empty()) return m_base;
    if (index + 1 >= m_keys.size()) return m_keys[index].value;

    const Keyframe& a = m_keys[index];
    const Keyframe& b = m_keys[index + 1];
    const double span = b.time - a.time;
    if (span <= kEpsilon) return b.value;

    double u = (t - a.time) / span;
    if (a.interp == Interp::Hold) return a.value;
    if (a.speed == 0.0 && b.speed == 0.0) return a.value;

    if (!a.isLinear()) {
        // Tangente de saida de 'a' e tangente de entrada de 'b'.
        u = cubicBezierEasing(u, b.easeIn, a.easeOut);
    }

    Value v = a.value.mix(b.value, u);
    clampToRange(v);
    return v;
}

Value Property::evaluate(Time t) const {
    ExpressionContext ctx;
    ctx.time = t;
    return evaluate(t, ctx);
}

Value Property::evaluate(Time t, const ExpressionContext& ctx) const {
    if (m_exprEnabled && !m_expression.empty()) {
        if (const auto backend = expressionBackend()) {
            Value result;
            ExpressionContext local = ctx;
            if (local.name.empty()) local.name = m_name;
            if (backend->evaluate(*this, local, result) && result.isValid()) {
                if (result.type() != m_type && m_type != ValueType::None) {
                    Value converted = result;
                    if (converted.convertTo(m_type)) {
                        clampToRange(converted);
                        return converted;
                    }
                }
                clampToRange(result);
                return result;
            }
        }
        // Expressao invalida: cai para a curva em vez de zerar o valor.
    }
    return sampleCurve(t);
}

uint32_t Property::keyframeIndexAt(Time t, double tolerance) const {
    for (size_t i = 0; i < m_keys.size(); ++i) {
        if (std::abs(m_keys[i].time - t) <= tolerance) return static_cast<uint32_t>(i);
    }
    return kNoIndex;
}

uint32_t Property::setKeyframe(Time t, const Value& v, bool preserveEase) {
    if (!m_animatable) return kNoIndex;

    Value stored = v;
    if (stored.type() != m_type && m_type != ValueType::None) {
        if (!stored.convertTo(m_type)) return kNoIndex;
    }
    clampToRange(stored);

    const uint32_t existing = keyframeIndexAt(t);
    if (existing != kNoIndex) {
        m_keys[existing].value = stored;
        m_dirty = true;
        return existing;
    }

    // Herda a suavidade do vizinho para que editar uma key nova nao fique dura.
    Keyframe k;
    k.time = t;
    k.value = std::move(stored);
    k.interp = Interp::Bezier;
    k.easeIn = 0.35;
    k.easeOut = 0.35;
    k.speed = 1.0;

    if (preserveEase && !m_keys.empty()) {
        const uint32_t idx = keyframeIndexAt(t - 1e-3);
        const Keyframe& near = (idx != kNoIndex) ? m_keys[idx] : m_keys.front();
        k.easeOut = near.easeOut;
    }

    const auto it = std::lower_bound(m_keys.begin(), m_keys.end(), t,
                                     [](const Keyframe& a, Time b) { return a.time < b; });
    const auto pos = static_cast<uint32_t>(std::distance(m_keys.begin(), it));
    m_keys.insert(it, std::move(k));
    m_dirty = true;
    return pos;
}

bool Property::removeKeyframeAt(Time t, double tolerance) {
    const uint32_t idx = keyframeIndexAt(t, tolerance);
    if (idx == kNoIndex) return false;
    m_keys.erase(m_keys.begin() + idx);
    m_dirty = true;
    return true;
}

void Property::sortKeyframes() {
    std::stable_sort(m_keys.begin(), m_keys.end(),
                     [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });
    m_dirty = true;
}

bool operator==(const Property& a, const Property& b) {
    return a.m_name == b.m_name && a.m_type == b.m_type && a.m_base == b.m_base &&
           a.m_expression == b.m_expression && a.m_keys.size() == b.m_keys.size();
}

}  // namespace lmn
