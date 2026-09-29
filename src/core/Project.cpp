#include "Project.h"

#include <algorithm>

#include "NodeRegistry.h"

namespace lmn {

Project::Project() {
    m_timeline.setRange(TimeRange{0.0, 10.0});
    m_output.size = m_size;
    m_output.fps = m_fps;
}

Composition* Project::composition(CompId id) {
    const auto it = m_comps.find(id);
    return it == m_comps.end() ? nullptr : &it->second;
}

const Composition* Project::composition(CompId id) const {
    const auto it = m_comps.find(id);
    return it == m_comps.end() ? nullptr : &it->second;
}

CompId Project::createComposition(std::string name, const Size& size, double fps,
                                   Time duration) {
    const CompId id = m_nextCompId++;
    m_comps.emplace(id, Composition(id, std::move(name), size, fps, duration));
    touch();
    return id;
}

bool Project::removeComposition(CompId id) {
    if (m_comps.size() <= 1) return false;  // o projeto sempre tem ao menos uma
    if (m_comps.erase(id) == 0) return false;

    // Limpa clips que apontavam para a composicao removida.
    for (auto& stack : m_timeline.stacks()) {
        std::vector<ClipId> doomed;
        for (const auto& c : stack.clips()) {
            if (c.comp == id) doomed.push_back(c.id);
        }
        for (ClipId cid : doomed) stack.removeClip(cid);
    }
    // Nos que referenciavam a composicao perdem a referencia.
    for (auto& [cid, comp] : m_comps) {
        for (NodeId nid : comp.graph().nodeIds()) {
            Node* n = comp.graph().node(nid);
            if (!n) continue;
            for (int s = 0; s < n->inputCount(); ++s) {
                const NodeId in = n->input(s);
                if (in != kInvalidId) {
                    const Node* src = comp.graph().node(in);
                    if (src && src->type() == "comp.reference") n->clearInput(s);
                }
            }
        }
    }
    touch();
    return true;
}

void Project::remapComposition(CompId from, CompId to) {
    for (auto& [cid, comp] : m_comps) {
        for (NodeId nid : comp.graph().nodeIds()) {
            Node* n = comp.graph().node(nid);
            if (!n) continue;
            for (int s = 0; s < n->inputCount(); ++s) {
                const NodeId in = n->input(s);
                const Node* src = comp.graph().node(in);
                if (!src || src->type() != "comp.reference") continue;
                if (Property* p = n->findProperty("composition")) {
                    if (static_cast<CompId>(p->baseValue().asDouble(0.0)) == from) {
                        p->setBaseValue(Value(static_cast<double>(to)));
                    }
                }
            }
        }
    }
    for (auto& stack : m_timeline.stacks()) {
        for (auto& c : stack.clips()) {
            if (c.comp == from) c.comp = to;
        }
    }
    touch();
}

CompId Project::duplicateComposition(CompId id, const std::string& newName) {
    const Composition* src = composition(id);
    if (!src) return 0;

    const CompId newId = createComposition(
        newName.empty() ? src->name() + " copia" : newName,
        src->size(), src->fps(), src->duration().out);

    Composition* dst = composition(newId);
    dst->setTransparentBackground(src->transparentBackground());
    dst->setBackground(src->background());
    dst->setWorkArea(src->workArea());

    // Ids sao preservados entre a copia e o original: cada composicao tem seu
    // proprio grafo, entao nao ha colisao e as arestas podem ser copiadas 1:1.
    for (NodeId nid : src->graph().nodeIds()) {
        const Node* n = src->graph().node(nid);
        if (!n) continue;
        Node copy = NodeRegistry::instance().create(n->type(), nid, n->name());
        copy.setEnabled(n->isEnabled());
        copy.setBlendMode(n->blendMode());
        copy.setCachePolicy(n->cachePolicy());
        copy.setGraphPos(n->graphPos());
        copy.setFlags(n->flags());
        copy.setPassIndex(n->passIndex());
        for (const auto& [pname, prop] : n->properties()) {
            Property p = prop;
            p.setName(pname);
            copy.addProperty(std::move(p));
        }
        dst->graph().addNode(std::move(copy));
    }
    for (NodeId nid : src->graph().nodeIds()) {
        const Node* n = src->graph().node(nid);
        Node* copy = dst->graph().node(nid);
        if (!n || !copy) continue;
        for (int s = 0; s < n->inputCount(); ++s) {
            const NodeId in = n->input(s);
            if (in != kInvalidId && dst->graph().contains(in)) {
                copy->setInput(s, in, n->inputOutputIndex(s));
            }
        }
        if (n->parent() != kInvalidId && dst->graph().contains(n->parent())) {
            copy->setParent(n->parent());
        }
    }
    dst->graph().setOutput(src->graph().output());
    touch();
    return newId;
}

std::vector<CompId> Project::compositionsUsedBy(CompId id) const {
    std::vector<CompId> out;
    for (const auto& [cid, comp] : m_comps) {
        if (cid == id) continue;
        bool used = false;
        for (NodeId nid : comp.graph().nodeIds()) {
            const Node* n = comp.graph().node(nid);
            if (!n) continue;
            const Property* p = n->findProperty("composition");
            if (p && static_cast<CompId>(p->baseValue().asDouble(0.0)) == id) {
                used = true;
                break;
            }
        }
        if (used) out.push_back(cid);
    }
    return out;
}

bool Project::compositionIsUsed(CompId id) const {
    for (const auto& [cid, comp] : m_comps) {
        if (cid == id) continue;
        for (NodeId nid : comp.graph().nodeIds()) {
            const Node* n = comp.graph().node(nid);
            if (!n) continue;
            const Property* p = n->findProperty("composition");
            if (p && static_cast<CompId>(p->baseValue().asDouble(0.0)) == id) return true;
        }
    }
    for (const auto& stack : m_timeline.stacks()) {
        for (const auto& c : stack.clips()) {
            if (c.comp == id) return true;
        }
    }
    return false;
}

void Project::setSize(const Size& s) {
    if (!s.isValid() || m_size == s) return;
    m_size = s;
    m_output.size = s;
    touch();
}

void Project::setFps(double f) {
    if (f <= 0.0 || m_fps == f) return;
    m_fps = f;
    m_output.fps = f;
    touch();
}

Frame Project::timeToFrame(Time t) const {
    if (t <= 0.0) return 0;
    return static_cast<Frame>(std::llround(t * m_fps));
}

Time Project::frameToTime(Frame f) const {
    return static_cast<Time>(f) / m_fps;
}

void Project::clear() {
    m_comps.clear();
    m_timeline = Timeline{};
    m_media.clear();
    m_nextCompId = 1;
    touch();
}

void Project::createDefaultComposition() {
    const CompId id = createComposition("Composicao 1", m_size, m_fps, 5.0);
    Composition* comp = composition(id);
    if (!comp) return;

    // Grafo minimo: Solid -> Merge -> Output. Ja move pixels no primeiro
    // projeto aberto, o que evita "tela preta sem explicacao".
    Node* solid = comp->graph().createNode("src.solid", "Fundo");
    if (solid) {
        solid->setGraphPos({-260.0, 0.0});
        if (Property* c = solid->findProperty("color")) {
            c->setBaseValue(Value(Color{0.12, 0.13, 0.15, 1.0}));
        }
        if (Property* s = solid->findProperty("size")) {
            s->setBaseValue(Value(Vec2{100.0, 100.0}));
        }
    }

    Node* merge = comp->graph().createNode("merge.over", "Merge");
    if (merge) {
        merge->setGraphPos({40.0, 0.0});
        if (solid) merge->setInput(0, solid->id());
    }

    comp->graph().setOutput(merge ? merge->id() : (solid ? solid->id() : kInvalidId));
    touch();
}

}  // namespace lmn
