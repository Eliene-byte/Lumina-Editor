#include "Node.h"

#include <algorithm>
#include <cmath>

namespace lmn {

// ---------------------------------------------------------------------------
// Transform
// ---------------------------------------------------------------------------

Transform Transform::translation(const Vec2& t) {
    Transform r;
    r.m[2] = t.x;
    r.m[5] = t.y;
    return r;
}

Transform Transform::scale(const Vec2& s) {
    Transform r;
    r.m[0] = s.x;
    r.m[4] = s.y;
    return r;
}

Transform Transform::rotationZ(double radians) {
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    Transform r;
    r.m[0] = c;  r.m[1] = -s;
    r.m[3] = s;  r.m[4] = c;
    return r;
}

Transform Transform::operator*(const Transform& o) const {
    Transform r;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            r.m[i * 3 + j] = m[i * 3 + 0] * o.m[0 * 3 + j] +
                             m[i * 3 + 1] * o.m[1 * 3 + j] +
                             m[i * 3 + 2] * o.m[2 * 3 + j];
        }
    }
    return r;
}

Vec2 Transform::applyPoint(const Vec2& p) const {
    return {m[0] * p.x + m[1] * p.y + m[2],
            m[3] * p.x + m[4] * p.y + m[5]};
}

Vec2 Transform::applyVector(const Vec2& v) const {
    return {m[0] * v.x + m[1] * v.y, m[3] * v.x + m[4] * v.y};
}

double Transform::determinant() const {
    return m[0] * (m[4] * m[8] - m[5] * m[7]) -
           m[1] * (m[3] * m[8] - m[5] * m[6]) +
           m[2] * (m[3] * m[7] - m[4] * m[6]);
}

bool Transform::invert(Transform& out) const {
    const double det = determinant();
    if (std::abs(det) < 1e-12) return false;

    const double inv = 1.0 / det;
    out = Transform{};
    out.m[0] = (m[4] * m[8] - m[5] * m[7]) * inv;
    out.m[1] = (m[2] * m[7] - m[1] * m[8]) * inv;
    out.m[2] = (m[1] * m[5] - m[2] * m[4]) * inv;
    out.m[3] = (m[5] * m[6] - m[3] * m[8]) * inv;
    out.m[4] = (m[0] * m[8] - m[2] * m[6]) * inv;
    out.m[5] = (m[2] * m[3] - m[0] * m[5]) * inv;
    out.m[6] = (m[3] * m[7] - m[4] * m[6]) * inv;
    out.m[7] = (m[1] * m[6] - m[0] * m[7]) * inv;
    out.m[8] = (m[0] * m[4] - m[1] * m[3]) * inv;
    return true;
}

Vec2 Transform::applyInversePoint(const Vec2& p) const {
    Transform inv;
    if (invert(inv)) return inv.applyPoint(p);
    return p;
}

bool Transform::hasSkew() const {
    return std::abs(m[1]) > 1e-9 && std::abs(m[3]) > 1e-9 &&
           std::abs(m[1] + m[3]) > 1e-6;
}

Vec2 Transform::translationPart() const { return {m[2], m[5]}; }

Vec2 Transform::scalePart() const {
    return {std::hypot(m[0], m[3]), std::hypot(m[1], m[4])};
}

double Transform::rotationZ() const { return std::atan2(m[3], m[0]); }

// ---------------------------------------------------------------------------
// CachePolicy
// ---------------------------------------------------------------------------

const char* cachePolicyName(CachePolicy c) noexcept {
    switch (c) {
        case CachePolicy::Never:  return "never";
        case CachePolicy::Frame:  return "frame";
        case CachePolicy::Region: return "region";
        case CachePolicy::Full:   return "full";
        case CachePolicy::Count:  break;
    }
    return "never";
}

CachePolicy cachePolicyFromName(const std::string& s) noexcept {
    if (s == "frame")  return CachePolicy::Frame;
    if (s == "region") return CachePolicy::Region;
    if (s == "full")   return CachePolicy::Full;
    return CachePolicy::Never;
}

// ---------------------------------------------------------------------------
// Node
// ---------------------------------------------------------------------------

Node::Node(NodeId id, std::string type, std::string name)
    : m_id(id), m_type(std::move(type)), m_name(std::move(name)) {}

std::string Node::displayName() const {
    if (!m_name.empty()) return m_name;
    return m_type + "#" + std::to_string(m_id);
}

void Node::setFlag(NodeFlag f, bool on) {
    if (on) m_flags |= f;
    else m_flags = static_cast<NodeFlag>(static_cast<uint32_t>(m_flags) & ~static_cast<uint32_t>(f));
}

const Property* Node::findProperty(const std::string& name) const {
    const auto it = m_props.find(name);
    return it == m_props.end() ? nullptr : &it->second;
}

Property* Node::findProperty(const std::string& name) {
    const auto it = m_props.find(name);
    return it == m_props.end() ? nullptr : &it->second;
}

Property& Node::property(const std::string& name) {
    auto it = m_props.find(name);
    if (it == m_props.end()) {
        Property p;
        p.setName(name);
        it = m_props.emplace(name, std::move(p)).first;
    }
    return it->second;
}

bool Node::addProperty(Property p) {
    if (p.name().empty() || m_props.count(p.name())) return false;
    m_props.emplace(p.name(), std::move(p));
    return true;
}

bool Node::removeProperty(const std::string& name) {
    return m_props.erase(name) > 0;
}

std::vector<std::string> Node::propertyNames() const {
    std::vector<std::string> out;
    out.reserve(m_props.size());
    for (const auto& [k, v] : m_props) out.push_back(k);
    return out;
}

double Node::opacityAt(Time t) const {
    const Property* p = findProperty("opacity");
    if (!p) return 1.0;
    return std::clamp(p->evaluate(t).asDouble(1.0), 0.0, 1.0);
}

void Node::setTransformProperties() {
    auto ensure = [this](const char* name, Value v, PropertyUi ui,
                         double lo, double hi, bool clamp) {
        if (m_props.count(name)) return;
        Property p(v, ui);
        p.setName(name);
        p.setRange(lo, hi);
        p.setClampsToRange(clamp);
        m_props.emplace(name, std::move(p));
    };

    ensure("position", Value(Vec2{0.0, 0.0}), PropertyUi::Position, -1e6, 1e6, false);
    ensure("scale", Value(Vec2{100.0, 100.0}), PropertyUi::Slider, 0.0, 10000.0, true);
    ensure("rotation", Value(0.0), PropertyUi::Angle, -1e6, 1e6, false);
    ensure("anchor", Value(Vec2{0.0, 0.0}), PropertyUi::Position, -1e6, 1e6, false);
    ensure("skew", Value(0.0), PropertyUi::Slider, -1e6, 1e6, false);
    ensure("skewAxis", Value(0.0), PropertyUi::Angle, -1e6, 1e6, false);
    ensure("opacity", Value(1.0), PropertyUi::Slider, 0.0, 1.0, true);
    ensure("blend", Value(std::string("Over")), PropertyUi::Enum, 0.0, 0.0, false);
}

Transform Node::transformAt(Time t) const {
    // Ordem: anchor -> skew -> rotation -> scale -> position (igual ao AE).
    const auto val = [this, t](const char* name, double fallback) {
        const Property* p = findProperty(name);
        return p ? p->evaluate(t).asDouble(fallback) : fallback;
    };
    const auto vec = [this, t](const char* name, const Vec2& fallback) {
        const Property* p = findProperty(name);
        return p ? p->evaluate(t).asVec2(fallback) : fallback;
    };

    const Vec2 anchor = vec("anchor", {});
    const Vec2 position = vec("position", {});
    const Vec2 scale = vec("scale", Vec2{100.0, 100.0});
    const double rotation = val("rotation", 0.0) * 3.14159265358979323846 / 180.0;
    const double skew = val("skew", 0.0) * 3.14159265358979323846 / 180.0;
    const double skewAxis = val("skewAxis", 0.0) * 3.14159265358979323846 / 180.0;

    Transform t1 = Transform::translation(-anchor);
    Transform t2 = Transform::scale({scale.x / 100.0, scale.y / 100.0});
    Transform t3 = Transform::rotationZ(rotation);
    Transform t4 = Transform::rotationZ(skewAxis);
    Transform t5 = Transform::scale({std::cos(skew), 1.0});
    Transform t6 = Transform::translation(position);

    return t6 * t3 * t4 * t5 * t4 * t2 * t1;
}

NodeId Node::input(int slot) const {
    if (slot < 0 || slot >= static_cast<int>(m_inputs.size())) return kInvalidId;
    return m_inputs[static_cast<size_t>(slot)].node;
}

void Node::setInput(int slot, NodeId source, int sourceOutput) {
    if (slot < 0) return;
    if (slot >= static_cast<int>(m_inputs.size())) m_inputs.resize(static_cast<size_t>(slot) + 1);
    m_inputs[static_cast<size_t>(slot)] = NodeInput{source, sourceOutput};
}

void Node::clearInput(int slot) {
    if (slot >= 0 && slot < static_cast<int>(m_inputs.size())) {
        m_inputs[static_cast<size_t>(slot)] = NodeInput{};
    }
}

bool Node::isConnected(int slot) const {
    if (slot < 0 || slot >= static_cast<int>(m_inputs.size())) return false;
    const NodeId id = m_inputs[static_cast<size_t>(slot)].node;
    return id != kInvalidId && id != m_id;
}

int Node::inputOutputIndex(int slot) const {
    if (slot < 0 || slot >= static_cast<int>(m_inputs.size())) return 0;
    return m_inputs[static_cast<size_t>(slot)].outputIndex;
}

void Node::setInputOutputIndex(int slot, int outIndex) {
    if (slot >= 0 && slot < static_cast<int>(m_inputs.size())) {
        m_inputs[static_cast<size_t>(slot)].outputIndex = std::max(0, outIndex);
    }
}

Node Node::cloned(NodeId newId) const {
    Node copy = *this;
    copy.m_id = newId;
    return copy;
}

void Node::copyPropertyValuesFrom(const Node& other) {
    for (auto& [name, prop] : m_props) {
        const Property* src = other.findProperty(name);
        if (!src) continue;
        Property merged = *src;
        merged.setName(name);
        prop = std::move(merged);
    }
}

void Node::redirectInput(NodeId from, NodeId to) {
    for (auto& in : m_inputs) {
        if (in.node == from) in.node = to;
    }
    if (m_parent == from) m_parent = to;
}

}  // namespace lmn
