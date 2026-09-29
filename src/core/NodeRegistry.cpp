#include "NodeRegistry.h"

#include <algorithm>

namespace lmn {

const PropertySpec* NodeTypeDesc::findProperty(const std::string& n) const {
    for (const auto& p : properties) {
        if (p.name == n) return &p;
    }
    return nullptr;
}

Property propertyFromSpec(const PropertySpec& spec) {
    Property p(spec.defaultValue, spec.ui);
    p.setName(spec.name);
    p.setRange(spec.min, spec.max);
    p.setClampsToRange(spec.clamps);
    p.setAnimatable(spec.animatable);
    p.setExpressionEnabled(spec.expressionEnabled);
    p.setGroup(spec.group);
    p.setTooltip(spec.tooltip);
    // Ajusta o tipo declarado quando o valor padrao e de outro tipo (ex.: um
    // spec de slider que usa Vec2 para Position).
    if (p.type() != spec.type && spec.type != ValueType::None) {
        p.setBaseValue(spec.defaultValue);
    }
    return p;
}

NodeRegistry& NodeRegistry::instance() {
    static NodeRegistry s_registry;
    return s_registry;
}

void NodeRegistry::registerType(const NodeTypeDesc& desc, NodeFactory factory) {
    NodeTypeDesc d = desc;
    if (d.displayName.empty()) d.displayName = d.type;
    if (d.outputNames.empty()) d.outputNames = {"Output"};

    Entry e;
    e.desc = std::move(d);
    e.factory = std::move(factory);
    m_types[e.desc.type] = std::move(e);
}

bool NodeRegistry::unregisterType(const std::string& type) {
    return m_types.erase(type) > 0;
}

const NodeTypeDesc* NodeRegistry::desc(const std::string& type) const {
    const auto it = m_types.find(type);
    return it == m_types.end() ? nullptr : &it->desc;
}

Node NodeRegistry::create(const std::string& type, NodeId id, const std::string& name) const {
    const auto it = m_types.find(type);
    if (it == m_types.end()) {
        return Node(id, type, name.empty() ? type : name);
    }

    const NodeTypeDesc& d = it->desc;
    Node node(id, d.type, name.empty() ? d.displayName : name);

    if (it->factory) {
        node = it->factory(id, name.empty() ? d.displayName : name);
        node.setName(name.empty() ? d.displayName : name);
    }

    for (const auto& spec : d.properties) {
        node.addProperty(propertyFromSpec(spec));
    }
    if (d.hasTransform) {
        node.setTransformProperties();
    }
    node.clearAllInputs();
    for (int i = 0; i < d.inputCount(); ++i) node.setInput(i, kInvalidId);

    return node;
}

std::vector<std::string> NodeRegistry::categories() const {
    std::vector<std::string> out;
    out.reserve(m_types.size());
    for (const auto& [type, e] : m_types) out.push_back(e.desc.category);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

std::vector<const NodeTypeDesc*> NodeRegistry::typesInCategory(const std::string& cat) const {
    std::vector<const NodeTypeDesc*> out;
    for (const auto& [type, e] : m_types) {
        if (e.desc.category == cat) out.push_back(&e.desc);
    }
    std::sort(out.begin(), out.end(),
              [](const NodeTypeDesc* a, const NodeTypeDesc* b) {
                  return a->displayName < b->displayName;
              });
    return out;
}

std::vector<const NodeTypeDesc*> NodeRegistry::allTypes() const {
    std::vector<const NodeTypeDesc*> out;
    out.reserve(m_types.size());
    for (const auto& [type, e] : m_types) out.push_back(&e.desc);
    std::sort(out.begin(), out.end(),
              [](const NodeTypeDesc* a, const NodeTypeDesc* b) {
                  return a->type < b->type;
              });
    return out;
}

}  // namespace lmn
