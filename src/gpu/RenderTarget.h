// RenderTarget.h - Texturas, framebuffers e o pool que evita alocar por frame.
//
// Alocar um FBO por passe e por frame e o que derruba um editor em maquina
// fraca. O pool mantem um conjunto de alvos do tamanho da composicao e recicla
// os que o grafo liberou.
#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <vector>

#include "GLContext.h"
#include "core/Types.h"

namespace lmn::gpu {

class RenderTarget;
using RenderTargetPtr = std::shared_ptr<RenderTarget>;

class RenderTarget {
public:
    RenderTarget() = default;
    ~RenderTarget();
    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    // Cria (ou recria) a textura e o FBO. Formato: GL_RGBA16F quando suportado,
    // GL_RGBA8 como reserva.
    bool create(int width, int height, GLenum internalFormat, bool linearFilter = true);
    void destroy();

    [[nodiscard]] bool valid() const { return m_fbo != 0; }
    [[nodiscard]] GLuint fbo() const { return m_fbo; }
    [[nodiscard]] GLuint texture() const { return m_tex; }
    [[nodiscard]] int width() const { return m_width; }
    [[nodiscard]] int height() const { return m_height; }
    [[nodiscard]] size_t byteSize() const;

    void bindAsTarget() const;
    void clear(const Color& c) const;

private:
    GLuint m_fbo = 0;
    GLuint m_tex = 0;
    int m_width = 0;
    int m_height = 0;
    GLenum m_format = 0;
};

class RenderTargetPool {
public:
    // Orcamento total de memoria de video para o pool. Acima disso os alvos
    // ociosos sao descartados - e o que segura o consumo em RAM.
    void setMemoryBudget(size_t bytes);
    [[nodiscard]] size_t memoryBudget() const { return m_budget; }
    void setFormat(GLenum format) { m_format = format; }
    [[nodiscard]] GLenum format() const { return m_format; }

    // Adquire um alvo do tamanho pedido, reciclando se possivel.
    [[nodiscard]] RenderTargetPtr acquire(int width, int height);

    // Devolve ao pool. O ponteiro e limpo para o chamador nao manter duas
    // referencias ao mesmo alvo.
    void release(RenderTargetPtr& target);

    // Fecha todos os alvos ociosos. Chamar ao trocar de composicao ou ao
    // reduzir o orcamento.
    void trim();
    void clear();

    [[nodiscard]] size_t idleCount() const { return m_idle.size(); }
    [[nodiscard]] size_t usedBytes() const;

private:
    void trimLocked();

    struct Key {
        int w, h;
        bool operator<(const Key& o) const { return w != o.w ? w < o.w : h < o.h; }
    };

    std::vector<RenderTargetPtr> m_idle;
    std::map<Key, std::vector<RenderTargetPtr>> m_idleBySize;
    size_t m_used = 0;
    size_t m_budget = 256ull * 1024 * 1024;   // 256 MB de video
    GLenum m_format = GL_RGBA16F;
    bool m_linear = true;
};

}  // namespace lmn::gpu
