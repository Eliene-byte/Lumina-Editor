#include "Composition.h"

#include <algorithm>
#include <cmath>

namespace lmn {

Composition::Composition(CompId id, std::string name, const Size& size, double fps,
                         Time duration)
    : m_id(id),
      m_name(std::move(name)),
      m_size(size),
      m_fps(fps > 0.0 ? fps : 30.0),
      m_duration(duration, duration) {
    m_workArea = m_duration;
}

void Composition::setWorkArea(TimeRange r) {
    // A area de trabalho nunca pode sair da duracao da composicao.
    const Time lo = std::min(r.in, m_duration.out);
    const Time hi = std::clamp(r.out, lo, m_duration.out);
    m_workArea = TimeRange{lo, hi};
}

Frame Composition::timeToFrame(Time t) const {
    if (t <= 0.0) return 0;
    return static_cast<Frame>(std::llround(t * m_fps));
}

Time Composition::frameToTime(Frame f) const {
    return static_cast<Time>(f) / m_fps;
}

Frame Composition::durationInFrames() const {
    return std::max<Frame>(1, timeToFrame(m_duration.duration()));
}

Node* Composition::findNode(const std::string& type) const {
    for (NodeId id : m_graph.nodeIds()) {
        Node* n = m_graph.node(id);
        if (n && n->type() == type) return n;
    }
    return nullptr;
}

std::vector<CompId> Composition::children() const {
    std::vector<CompId> out;
    for (NodeId id : m_graph.nodeIds()) {
        const Node* n = m_graph.node(id);
        if (!n) continue;
        if (const Property* p = n->findProperty("composition")) {
            const auto v = p->baseValue().asDouble(0.0);
            if (v > 0.0) out.push_back(static_cast<CompId>(v));
        }
    }
    return out;
}

}  // namespace lmn
