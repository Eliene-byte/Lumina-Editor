#include "NodeUniforms.h"

#include <cstring>

#include "core/NodeRegistry.h"

namespace lmn::gpu {
namespace {

const Property* findProperty(const Node& node, const char* name) {
    return node.findProperty(name);
}

}  // namespace

NodeUniformRegistry& NodeUniformRegistry::instance() {
    static NodeUniformRegistry registry;
    return registry;
}

void NodeUniformRegistry::registerBinder(const std::string& nodeType, UniformBinder binder) {
    m_binders[nodeType] = std::move(binder);
}

UniformBinder NodeUniformRegistry::find(const std::string& nodeType) const {
    const auto it = m_binders.find(nodeType);
    return it == m_binders.end() ? UniformBinder{} : it->second;
}

bool NodeUniformRegistry::has(const std::string& nodeType) const {
    return m_binders.count(nodeType) > 0;
}

void NodeUniformRegistry::clear() {
    m_binders.clear();
}

void setPropertyF(ShaderProgram& p, const Node& node, const char* uniform, Time t,
                  const ExpressionContext& ctx) {
    if (const Property* prop = findProperty(node, uniform)) {
        p.setFloat(uniform, static_cast<float>(prop->evaluate(t, ctx).asDouble(0.0)));
    }
}

void setPropertyI(ShaderProgram& p, const Node& node, const char* uniform, Time t,
                  const ExpressionContext& ctx) {
    if (const Property* prop = findProperty(node, uniform)) {
        p.setInt(uniform, static_cast<int>(prop->evaluate(t, ctx).asDouble(0.0)));
    }
}

void setPropertyV2(ShaderProgram& p, const Node& node, const char* uniform, Time t,
                   const ExpressionContext& ctx) {
    if (const Property* prop = findProperty(node, uniform)) {
        p.setVec2(uniform, prop->evaluate(t, ctx).asVec2());
    }
}

void setPropertyV3(ShaderProgram& p, const Node& node, const char* uniform, Time t,
                   const ExpressionContext& ctx) {
    if (const Property* prop = findProperty(node, uniform)) {
        p.setVec3(uniform, prop->evaluate(t, ctx).asVec3());
    }
}

void setPropertyV4(ShaderProgram& p, const Node& node, const char* uniform, Time t,
                   const ExpressionContext& ctx) {    if (const Property* prop = findProperty(node, uniform)) {
        p.setVec4(uniform, prop->evaluate(t, ctx).toVec4());
    }
}

void setPropertyColor(ShaderProgram& p, const Node& node, const char* uniform, Time t,
                      const ExpressionContext& ctx) {
    if (const Property* prop = findProperty(node, uniform)) {
        p.setColor(uniform, prop->evaluate(t, ctx).asColor());
    }
}

void setPropertyBool(ShaderProgram& p, const Node& node, const char* uniform, Time t,
                     const ExpressionContext& ctx) {
    if (const Property* prop = findProperty(node, uniform)) {
        p.setBool(uniform, prop->evaluate(t, ctx).asBool(false));
    }
}

void setPropertyEnum(ShaderProgram& p, const Node& node, const char* uniform, Time t,
                     const ExpressionContext& ctx, const char* const* options,
                     int optionCount, int fallback) {
    const Property* prop = findProperty(node, uniform);
    if (!prop) {
        p.setInt(uniform, fallback);
        return;
    }
    const std::string value = prop->evaluate(t, ctx).asString();
    for (int i = 0; i < optionCount; ++i) {
        if (value == options[i]) {
            p.setInt(uniform, i);
            return;
        }
    }
    p.setInt(uniform, fallback);
}

}  // namespace lmn::gpu
