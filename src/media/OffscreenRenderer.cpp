#include "OffscreenRenderer.h"

#include <QOpenGLFunctions_3_3_Core>
#include <QSurfaceFormat>

#include <algorithm>

#include "gpu/NodeUniforms.h"

namespace lmn::media {
namespace {

// VAO do triangulo de tela cheia, local a esta thread. O Compositor tem o seu
// proprio, mas aqui o desenho e feito diretamente.
void drawFullscreen() {
    static thread_local GLuint vao = 0;
    if (!vao) {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glBindVertexArray(0);
    }
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

}  // namespace

OffscreenRenderer::OffscreenRenderer() = default;

OffscreenRenderer::~OffscreenRenderer() {
    if (m_context.makeCurrent(&m_surface)) {
        m_pool.clear();
        m_context.doneCurrent();
    }
}

bool OffscreenRenderer::initialize(std::string* error) {
    if (m_initialized) return true;

    // Formato sem alpha no surface, RGBA16F nos alvos internos: o alpha e do
    // conteudo, nao da superficie.
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(0);
    format.setStencilBufferSize(0);
    format.setSamples(0);
    m_surface.setFormat(format);
    m_surface.create();

    if (!m_surface.isValid()) {
        if (error) *error = "nao foi possivel criar uma superficie OpenGL offscreen";
        return false;
    }

    // Compartilhamento de contexto nao e necessario: cada thread de exportacao
    // cria o seu. Sem isso, um QOpenGLContext so pode estar corrente em uma
    // thread por vez.
    m_context.setShareContext(nullptr);
    if (!m_context.create()) {
        if (error) {
            *error =
                "OpenGL 3.3 indisponivel. A GPU ou o driver nao suporta o perfil "
                "necessario para a exportacao";
        }
        return false;
    }

    if (!m_context.makeCurrent(&m_surface)) {
        if (error) *error = "nao foi possivel ativar o contexto OpenGL";
        return false;
    }

    if (!m_gl.initialize()) {
        m_context.doneCurrent();
        if (error) *error = "as funcoes OpenGL nao puderam ser carregadas";
        return false;
    }

    m_renderer = m_gl.rendererString();
    m_software = m_gl.caps().isSoftware;
    m_maxTexture = m_gl.caps().maxTextureSize;

    m_pool.setFormat(m_gl.caps().hasHalfFloat ? GL_RGBA16F : GL_RGBA8);
    m_pool.setMemoryBudget(256ull * 1024 * 1024);

    m_initialized = true;
    return true;
}

bool OffscreenRenderer::makeCurrent() {
    return m_context.makeCurrent(&m_surface);
}

void OffscreenRenderer::releaseCurrent() {
    m_context.doneCurrent();
}

std::vector<uint8_t> OffscreenRenderer::renderFrame(gpu::Compositor& compositor,
                                                    const Composition& comp, Time time,
                                                    int width, int height,
                                                    const Project* project) {
    std::vector<uint8_t> out;
    if (!m_initialized || !makeCurrent()) return out;

    // Clamp na textura: um projeto 8K em uma GPU de 2048 nao cabe, e encolher
    // e melhor do que renderizar errado.
    int w = std::min(width, m_maxTexture);
    int h = std::min(height, m_maxTexture);

    if (!m_renderContext || m_renderContext->width() != w ||
        m_renderContext->height() != h) {
        m_renderContext = std::make_unique<gpu::RenderContext>(m_pool, w, h);
    }

    const GLuint texture = compositor.render(
        const_cast<Composition&>(comp), time, *m_renderContext, project, nullptr);
    if (!texture) {
        m_renderContext->releaseAll();
        releaseCurrent();
        return out;
    }

    // Le de volta para a CPU. Um map buffer seria mais rapido, mas
    // glReadPixels funciona em qualquer driver e a diferenca nao justifica
    // um segundo caminho de codigo.
    out.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);

    std::vector<uint8_t> raw(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    m_renderContext->releaseAll();
    releaseCurrent();

    if (w == width && h == height) {
        return raw;
    }

    // Reescalado no CPU. O caminho feliz nunca passa por aqui: so acontece em
    // projeto maior que a textura maxima da GPU. Perto o suficiente para o
    // caso ser raro; nearest neighbor daria serrilha visivel.
    for (int y = 0; y < height; ++y) {
        const int sy = y * h / height;
        for (int x = 0; x < width; ++x) {
            const int sx = x * w / width;
            const size_t src = (static_cast<size_t>(sy) * w + sx) * 4;
            const size_t dst = (static_cast<size_t>(y) * width + x) * 4;
            out[dst + 0] = raw[src + 0];
            out[dst + 1] = raw[src + 1];
            out[dst + 2] = raw[src + 2];
            out[dst + 3] = raw[src + 3];
        }
    }
    return out;
}

}  // namespace lmn::media
