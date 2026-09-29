#include "Timeline.h"

#include <algorithm>

namespace lmn {

const char* transitionTypeName(TransitionType t) noexcept {
    switch (t) {
        case TransitionType::None:          return "none";
        case TransitionType::CrossDissolve: return "cross";
        case TransitionType::DipToBlack:    return "dipToBlack";
        case TransitionType::DipToWhite:    return "dipToWhite";
        case TransitionType::WipeLeft:      return "wipeLeft";
        case TransitionType::WipeRight:     return "wipeRight";
        case TransitionType::Push:          return "push";
        case TransitionType::Slide:         return "slide";
        case TransitionType::Zoom:          return "zoom";
        case TransitionType::Count:         break;
    }
    return "none";
}

TransitionType transitionTypeFromName(const std::string& s) noexcept {
    for (uint8_t i = 0; i < static_cast<uint8_t>(TransitionType::Count); ++i) {
        if (s == transitionTypeName(static_cast<TransitionType>(i))) {
            return static_cast<TransitionType>(i);
        }
    }
    return TransitionType::None;
}

void Clip::setDuration(Time d) {
    d = std::max(0.0, d);
    if (speed > kEpsilon) {
        sourceOut = sourceIn + d / speed;
    } else {
        speed = 1.0;
        sourceOut = sourceIn + d;
    }
}

// ---------------------------------------------------------------------------
// Stack
// ---------------------------------------------------------------------------

Stack::Stack(StackId id, std::string name, int index)
    : m_id(id), m_name(std::move(name)), m_index(index) {}

Clip* Stack::findClip(ClipId id) {
    for (auto& c : m_clips) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

const Clip* Stack::findClip(ClipId id) const {
    for (const auto& c : m_clips) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

Clip* Stack::insert(Clip clip) {
    if (clip.id == 0) clip.id = 1;
    if (clip.sourceOut < clip.sourceIn) clip.sourceOut = clip.sourceIn;

    const auto pos = std::lower_bound(
        m_clips.begin(), m_clips.end(), clip.start,
        [](const Clip& a, Time t) { return a.start < t; });
    return &*m_clips.insert(pos, std::move(clip));
}

bool Stack::removeClip(ClipId id) {
    const auto it = std::find_if(m_clips.begin(), m_clips.end(),
                                 [id](const Clip& c) { return c.id == id; });
    if (it == m_clips.end()) return false;
    m_clips.erase(it);
    return true;
}

TimeRange Stack::range() const {
    if (m_clips.empty()) return TimeRange{0.0, 0.0};
    Time in = m_clips.front().start;
    Time out = in;
    for (const auto& c : m_clips) {
        in = std::min(in, c.start);
        out = std::max(out, c.end());
    }
    return TimeRange{in, out};
}

std::vector<const Clip*> Stack::clipsAt(Time t) const {
    std::vector<const Clip*> out;
    for (const auto& c : m_clips) {
        if (c.contains(t)) out.push_back(&c);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Timeline
// ---------------------------------------------------------------------------

Stack* Timeline::addStack(std::string name) {
    StackId id = 1;
    for (const auto& s : m_stacks) id = std::max<StackId>(id, s.id + 1);
    if (name.empty()) name = "V" + std::to_string(m_stacks.size() + 1);
    m_stacks.emplace_back(id, std::move(name), static_cast<int>(m_stacks.size()));
    return &m_stacks.back();
}

bool Timeline::removeStack(StackId id) {
    const auto it = std::find_if(m_stacks.begin(), m_stacks.end(),
                                 [id](const Stack& s) { return s.id == id; });
    if (it == m_stacks.end()) return false;
    m_stacks.erase(it);
    reindexStacks();
    return true;
}

Stack* Timeline::findStack(StackId id) {
    for (auto& s : m_stacks) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

const Stack* Timeline::findStack(StackId id) const {
    for (const auto& s : m_stacks) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

Clip* Timeline::findClip(ClipId id) {
    for (auto& s : m_stacks) {
        if (Clip* c = s.findClip(id)) return c;
    }
    return nullptr;
}

Timeline::LocatedClip Timeline::locate(ClipId id) {
    for (auto& s : m_stacks) {
        if (Clip* c = s.findClip(id)) return LocatedClip{&s, c};
    }
    return LocatedClip{nullptr, nullptr};
}

void Timeline::removeClipEverywhere(ClipId id) {
    for (auto& s : m_stacks) s.removeClip(id);
}

void Timeline::reindexStacks() {
    for (size_t i = 0; i < m_stacks.size(); ++i) m_stacks[i].setIndex(static_cast<int>(i));
}

TimeRange Timeline::contentRange() const {
    bool first = true;
    Time in = 0.0, out = 0.0;
    for (const auto& s : m_stacks) {
        if (s.empty()) continue;
        const TimeRange r = s.range();
        if (r.duration() <= 0.0) continue;
        if (first) { in = r.in; out = r.out; first = false; }
        else { in = std::min(in, r.in); out = std::max(out, r.out); }
    }
    return first ? TimeRange{0.0, 0.0} : TimeRange{in, out};
}

void Timeline::fitToContent(double marginSeconds) {
    const TimeRange r = contentRange();
    if (r.duration() <= 0.0) return;
    m_range = TimeRange{std::max(0.0, r.in - marginSeconds), r.out + marginSeconds};
}

}  // namespace lmn
