#include "SourceProviderGL.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>

#include <algorithm>

namespace lmn::media {

SourceProviderGL::SourceProviderGL(Project& project, MediaBackend& backend)
    : m_project(project), m_backend(backend) {}

SourceProviderGL::~SourceProviderGL() {
    shutdown();
}

void SourceProviderGL::initialize() {
    if (m_initialized) return;
    m_pool = m_backend.createSourcePool();
    m_initialized = true;
}

void SourceProviderGL::shutdown() {
    destroyAll();
    if (m_pool) {
        m_pool->clear();
        m_pool.reset();
    }
    m_initialized = false;
}

void SourceProviderGL::invalidateAll() {
    std::lock_guard lock(m_mutex);
    destroyAll();
}

void SourceProviderGL::destroyAll() {
    for (auto& [key, entry] : m_textures) {
        if (entry.texture) glDeleteTextures(1, &entry.texture);
        if (m_pool && entry.source) m_pool->release(entry.source);
    }
    m_textures.clear();
}

size_t SourceProviderGL::bytesUsed() const {
    std::lock_guard lock(m_mutex);
    size_t total = 0;
    for (const auto& [key, entry] : m_textures) total += entry.bytes;
    return total;
}

std::string SourceProviderGL::cacheKey(NodeId node, MediaId media, bool useProxy) {
    return std::to_string(node) + "|" + std::to_string(media) + "|" +
           (useProxy ? "p" : "f");
}

SourceProviderGL::Entry* SourceProviderGL::findEntry(const std::string& key) {
    const auto it = m_textures.find(key);
    return it == m_textures.end() ? nullptr : &it->second;
}

GLuint SourceProviderGL::upload(Entry& entry, const VideoFrame& frame) {
    if (!frame.valid()) return 0;

    if (!entry.texture) {
        glGenTextures(1, &entry.texture);
        if (!entry.texture) return 0;
        glBindTexture(GL_TEXTURE_2D, entry.texture);

        // As texturas de video nao tem mipmap: para, e um ganho minimo que
        // custa um upload extra por quadro.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, frame.width, frame.height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, frame.data.data());
    }

    if (entry.width != frame.width || entry.height != frame.height) {
        glBindTexture(GL_TEXTURE_2D, entry.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, frame.width, frame.height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, frame.data.data());
    } else {
        // glTexSubImage2D so escreve a regiao alterada: em geral o mesmo
        // tamanho, entao evita realocar o armazenamento interno.
        glBindTexture(GL_TEXTURE_2D, entry.texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.width, frame.height, GL_RGBA,
                        GL_UNSIGNED_BYTE, frame.data.data());
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    entry.width = frame.width;
    entry.height = frame.height;
    entry.bytes = static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height) * 4;
    entry.lastFrame = frame.frameIndex;
    return entry.texture;
}

GLuint SourceProviderGL::textureFor(const Request& request) {
    if (!m_initialized || !m_pool || !request.nodeRef) return 0;

    const Node& node = *request.nodeRef;
    const Property* mediaProp = node.findProperty("mediaId");
    if (!mediaProp) return 0;

    const auto mediaId = static_cast<MediaId>(mediaProp->baseValue().asDouble(0.0));
    if (mediaId == 0) return 0;

    const MediaItem* item = m_project.media().find(mediaId);
    if (!item) return 0;

    std::lock_guard lock(m_mutex);

    const bool useProxy = shouldUseProxy(*item, m_useProxy);
    const std::string path = useProxy ? item->proxyPath : item->path;
    if (path.empty()) return 0;

    const std::string key = cacheKey(node.id(), mediaId, useProxy);
    Entry* entry = findEntry(key);
    if (!entry) {
        Entry fresh;
        fresh.media = mediaId;
        fresh.path = path;
        m_textures[key] = std::move(fresh);
        entry = findEntry(key);
        if (!entry) return 0;
    }

    if (!entry->source) {
        entry->source = m_pool->acquire(*item);
        if (!entry->source) return 0;
        std::string error;
        if (!entry->source->open(path, &error)) {
            m_pool->release(entry->source);
            entry->source.reset();
            return 0;
        }
        if (useProxy) entry->source->setUseProxy(true);
    }

    const VideoFrame* frame = entry->source->frameAt(request.time);
    if (!frame) return 0;
    return upload(*entry, *frame);
}

void SourceProviderGL::prefetch(const Request& request) {
    if (!m_initialized || !m_pool || !request.nodeRef) return;

    const Property* mediaProp = request.nodeRef->findProperty("mediaId");
    if (!mediaProp) return;
    const auto mediaId = static_cast<MediaId>(mediaProp->baseValue().asDouble(0.0));
    if (mediaId == 0) return;
    const MediaItem* item = m_project.media().find(mediaId);
    if (!item) return;

    const bool useProxy = shouldUseProxy(*item, m_useProxy);
    const std::string path = useProxy ? item->proxyPath : item->path;
    if (path.empty()) return;

    // Avisar o decoder e barato (so enfileira) e evita que o proximo frame
    // espere por decodificacao sincrona.
    if (auto source = m_pool->acquire(*item)) {
        source->prefetch(request.time);
        m_pool->release(source);
    }
}

}  // namespace lmn::media
