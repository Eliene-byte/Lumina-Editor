#include "MediaPool.h"

#include <algorithm>
#include <filesystem>

namespace lmn {

const char* mediaTypeName(MediaType t) noexcept {
    switch (t) {
        case MediaType::Unknown:     return "unknown";
        case MediaType::Video:       return "video";
        case MediaType::Audio:       return "audio";
        case MediaType::Image:       return "image";
        case MediaType::Solid:       return "solid";
        case MediaType::Composition: return "composition";
        case MediaType::NodeGroup:   return "nodeGroup";
        case MediaType::Lut:         return "lut";
        case MediaType::Font:        return "font";
        case MediaType::Count:       break;
    }
    return "unknown";
}

MediaType mediaTypeFromName(const std::string& s) noexcept {
    for (uint8_t i = 0; i < static_cast<uint8_t>(MediaType::Count); ++i) {
        if (s == mediaTypeName(static_cast<MediaType>(i))) return static_cast<MediaType>(i);
    }
    return MediaType::Unknown;
}

MediaItem* MediaPool::find(MediaId id) {
    for (auto& m : m_items) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

const MediaItem* MediaPool::find(MediaId id) const {
    for (const auto& m : m_items) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

MediaItem* MediaPool::findByPath(const std::string& path) {
    for (auto& m : m_items) {
        if (m.path == path) return &m;
    }
    return nullptr;
}

MediaId MediaPool::add(MediaItem item) {
    if (item.id == 0) item.id = m_nextId++;
    else m_nextId = std::max(m_nextId, item.id + 1);

    if (item.name.empty() && !item.path.empty()) {
        item.name = std::filesystem::path(item.path).stem().string();
    }
    m_items.push_back(std::move(item));
    return m_items.back().id;
}

bool MediaPool::remove(MediaId id) {
    const auto it = std::find_if(m_items.begin(), m_items.end(),
                                 [id](const MediaItem& m) { return m.id == id; });
    if (it == m_items.end()) return false;
    m_items.erase(it);
    return true;
}

std::vector<const MediaItem*> MediaPool::needsProxy(double maxWidth) const {
    std::vector<const MediaItem*> out;
    for (const auto& m : m_items) {
        if (m.type != MediaType::Video) continue;
        if (m.hasProxy()) continue;
        if (m.size.width <= maxWidth) continue;
        out.push_back(&m);
    }
    return out;
}

size_t MediaPool::proxyFileCount() const {
    size_t n = 0;
    for (const auto& m : m_items) {
        if (m.hasProxy()) ++n;
    }
    return n;
}

void MediaPool::setUseProxy(bool on) {
    m_useProxy = on;
    for (auto& m : m_items) m.useProxy = on;
}

void MediaPool::setProxyScale(double s) {
    s = std::clamp(s, 0.05, 1.0);
    for (auto& m : m_items) m.proxyScale = s;
}

void MediaPool::removeMissing(bool* modified) {
    for (auto& m : m_items) {
        if (m.type != MediaType::Video && m.type != MediaType::Audio &&
            m.type != MediaType::Image) {
            continue;
        }
        if (m.path.empty()) continue;

        std::error_code ec;
        const bool gone = !std::filesystem::exists(m.path, ec);
        if (gone && !m.isOffline) {
            m.isOffline = true;
            if (modified) *modified = true;
        } else if (!gone && m.isOffline) {
            m.isOffline = false;
            if (modified) *modified = true;
        }
    }
}

}  // namespace lmn
