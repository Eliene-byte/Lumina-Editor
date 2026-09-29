// OffscreenRenderer.h - Renderiza sem janela.
//
// A exportacao nao pode depender do visualizador: o preview pode estar em
// 50% de escala, o grafo pode ter cache de outro tempo, e o usuario pode ter
// fechado o painel. Este contexto e o unico caminho confiavel para gerar
// pixels de tamanho final.
//
// A solucao e um QOffscreenSurface com um QOpenGLContext proprio. Em Windows
// 10 antigo isso ainda usa o driver de hardware; se o driver for por software,
// cai para o render por CPU, que e lento mas sempre funciona.
#pragma once

#include <memory>

#include <QOffscreenSurface>
#include <QOpenGLContext>

#include "gpu/Compositor.h"
#include "gpu/GLContext.h"
#include "gpu/RenderTarget.h"

namespace lmn::media {

class OffscreenRenderer {
public:
    OffscreenRenderer();
    ~OffscreenRenderer();
    OffscreenRenderer(const OffscreenRenderer&) = delete;
    OffscreenRenderer& operator=(const OffscreenRenderer&) = delete;

    // Cria o contexto. Retorna false e preenche 'error' se nao houver GL
    // disponivel (por exemplo, em um servidor sem placa de video).
    bool initialize(std::string* error);

    // Garante um contexto corrente nesta thread. Precisa ser chamado em toda
    // thread que renderiza; o contexto nao e compartilhado entre threads.
    bool makeCurrent();
    void releaseCurrent();

    // Renderiza um frame e devolve RGBA de 8 bits por canal, tightly packed,
    // com origem no canto superior esquerdo (ordem de leitura usual em PNG).
    // Devolve nullptr se o grafo nao produziu nada.
    [[nodiscard]] std::vector<uint8_t> renderFrame(gpu::Compositor& compositor,
                                                   const class Composition& comp,
                                                   Time time, int width, int height,
                                                   const class Project* project = nullptr);

    [[nodiscard]] gpu::GLContext& gl() { return m_gl; }
    [[nodiscard]] const QString& rendererName() const { return m_renderer; }
    [[nodiscard]] bool isSoftware() const { return m_software; }
    [[nodiscard]] int maxTextureSize() const { return m_maxTexture; }

private:
    QOffscreenSurface m_surface;
    QOpenGLContext m_context;
    gpu::GLContext m_gl;
    gpu::RenderTargetPool m_pool;
    std::unique_ptr<gpu::RenderContext> m_renderContext;
    QString m_renderer;
    bool m_initialized = false;
    bool m_software = false;
    int m_maxTexture = 2048;
};

}  // namespace lmn::media
