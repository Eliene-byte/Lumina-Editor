#include "RenderTarget.h"

#include <algorithm>
#include <QMutex>
#include <QMutexLocker>

namespace lmn::gpu {
namespace {

QMutex& poolMutex() {
    static QMutex m;
    return m;
}

}  // namespace

RenderTarget::~RenderTarget() {
    destroy();
}

bool RenderTarget::create(int width, int height, GLenum internalFormat, bool linearFilter) {
    if (width <= 0 || height <= 0) return false;
    if (valid() && m_width == width && m_height == height && m_format == internalFormat) {
        return true;   // ja esta do tamanho certo
    }
    destroy();

    glGenTextures(1, &m_tex);
    if (!m_tex) return false;

    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat), width, height, 0,
                 GL_RGBA, (internalFormat == GL_RGBA8) ? GL_UNSIGNED_BYTE : GL_FLOAT,
                 nullptr);

    const GLint filter = linearFilter ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    // Clamp: evita borrar a borda quando um efeito amostra fora da imagem.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &m_fbo);
    if (!m_fbo) {
        glDeleteTextures(1, &m_tex);
        m_tex = 0;
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_tex, 0);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        destroy();
        return false;
    }

    m_width = width;
    m_height = height;
    m_format = internalFormat;
    return true;
}

void RenderTarget::destroy() {
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_tex) {
        glDeleteTextures(1, &m_tex);
        m_tex = 0;
    }
    m_width = m_height = 0;
    m_format = 0;
}

size_t RenderTarget::byteSize() const {
    const size_t bytesPerPixel = (m_format == GL_RGBA8) ? 4 : 8;
    return static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * bytesPerPixel;
}

void RenderTarget::bindAsTarget() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
}

void RenderTarget::clear(const Color& c) const {
    glClearColor(static_cast<GLfloat>(c.r), static_cast<GLfloat>(c.g),
                 static_cast<GLfloat>(c.b), static_cast<GLfloat>(c.a));
    glClear(GL_COLOR_BUFFER_BIT);
}

// ---------------------------------------------------------------------------
// Pool
// ---------------------------------------------------------------------------

void RenderTargetPool::setMemoryBudget(size_t bytes) {
    QMutexLocker lock(&poolMutex());
    m_budget = std::max<size_t>(bytes, 32ull * 1024 * 1024);
    trimLocked();
}

void RenderTargetPool::trimLocked() {
    while (m_used > m_budget && !m_idle.empty()) {
        const RenderTargetPtr& last = m_idle.back();
        const size_t sz = last->byteSize();
        const Key key{last->width(), last->height()};
        m_idle.pop_back();

        auto it = m_idleBySize.find(key);
        if (it != m_idleBySize.end() && !it->second.empty()) it->second.pop_back();
        m_used = (sz < m_used) ? m_used - sz : 0;
    }
}

RenderTargetPtr RenderTargetPool::acquire(int width, int height) {
    QMutexLocker lock(&poolMutex());
    const Key key{width, height};

    auto it = m_idleBySize.find(key);
    if (it != m_idleBySize.end() && !it->second.empty()) {
        RenderTargetPtr target = std::move(it->second.back());
        it->second.pop_back();
        m_idle.erase(std::remove(m_idle.begin(), m_idle.end(), target), m_idle.end());
        return target;
    }

    auto target = std::make_shared<RenderTarget>();
    if (!target->create(width, height, m_format, m_linear)) return nullptr;
    m_used += target->byteSize();
    return target;
}

void RenderTargetPool::release(RenderTargetPtr& target) {
    if (!target) return;
    QMutexLocker lock(&poolMutex());
    if (target->valid()) {
        m_idle.push_back(target);
        m_idleBySize[Key{target->width(), target->height()}].push_back(target);
    }
    target.reset();
    trimLocked();
}

void RenderTargetPool::trim() {
    QMutexLocker lock(&poolMutex());
    trimLocked();
}

void RenderTargetPool::clear() {
    QMutexLocker lock(&poolMutex());
    m_idle.clear();
    m_idleBySize.clear();
    m_used = 0;
}

size_t RenderTargetPool::usedBytes() const {
    QMutexLocker lock(&poolMutex());
    return m_used;
}

}  // namespace lmn::gpu
