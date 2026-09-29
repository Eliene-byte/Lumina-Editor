// TestGpu.cpp - Testes que exigem um contexto OpenGL.
//
// SAO PULADOS quando nao ha contexto disponivel (CI sem GPU, container sem
// display). O workflow do GitHub Actions cria um contexto de software com
// Mesa, entao la estes testes rodam de verdade.
//
// A distincao e importante: testar o compositor com um substituto de software
// da mais confianca no caminho de CPU do driver, que e justamente o que roda
// num PC modesto.
#include "TestSupport.h"

#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSurfaceFormat>

#include "core/Project.h"
#include "gpu/Compositor.h"
#include "gpu/GLContext.h"
#include "gpu/RenderTarget.h"
#include "nodes/Nodes.h"

using namespace lmn;

namespace {

// Contexto compartilhado: criar um por teste custa alguns ms e nao paga.
class GLFixture {
public:
    static bool available() {
        static const bool ok = create();
        return ok;
    }

    static QString renderer() {
        available();
        return s_renderer;
    }

    static bool makeCurrent() {
        if (!available()) return false;
        return s_context->makeCurrent(&s_surface);
    }

    static void release() {
        if (available()) s_context->doneCurrent();
    }

    [[nodiscard]] static gpu::GLContext& gl() {
        available();
        return s_gl;
    }

private:
    static bool create() {
        static bool tried = false;
        static bool result = false;
        if (tried) return result;
        tried = true;

        QSurfaceFormat format;
        format.setRenderableType(QSurfaceFormat::OpenGL);
        format.setVersion(3, 3);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setDepthBufferSize(0);
        format.setStencilBufferSize(0);

        s_surface = std::make_unique<QOffscreenSurface>();
        s_surface->setFormat(format);
        s_surface->create();

        if (!s_surface->isValid()) return false;

        s_context = std::make_unique<QOpenGLContext>();
        s_context->setFormat(format);
        if (!s_context->create()) return false;
        if (!s_context->makeCurrent(s_surface.get())) return false;

        if (!s_gl.initialize()) {
            s_context->doneCurrent();
            return false;
        }

        s_renderer = QString::fromLatin1(
            reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        result = true;
        return true;
    }

    static std::unique_ptr<QOffscreenSurface> s_surface;
    static std::unique_ptr<QOpenGLContext> s_context;
    static gpu::GLContext s_gl;
    static QString s_renderer;
};

std::unique_ptr<QOffscreenSurface> GLFixture::s_surface;
std::unique_ptr<QOpenGLContext> GLFixture::s_context;
gpu::GLContext GLFixture::s_gl;
QString GLFixture::s_renderer;

}  // namespace

LUMINA_TEST(Gpu, contextoDisponivel) {
    // Se este teste falhar, os demais vao pular: e o ponto de partida.
    if (!GLFixture::available()) {
        // Nao e falha: e o caminho esperado em um ambiente sem GPU. A suite
        // inteira de GPU e ignorada e o CI de software assume a cobertura.
        std::printf("        (sem contexto OpenGL 3.3 - testes de GPU ignorados)\n");
        return;
    }
    CHECK(!GLFixture::renderer().isEmpty());
    std::printf("        renderer: %s\n", GLFixture::renderer().toUtf8().constData());
}

LUMINA_TEST(Gpu, renderTargetCicloDeVida) {
    if (!GLFixture::available()) return;
    CHECK(GLFixture::makeCurrent());
    GLFixture::gl().initialize();

    {
        gpu::RenderTarget target;
        CHECK(target.create(256, 128, GL_RGBA16F));
        CHECK(target.valid());
        CHECK_EQ(target.width(), 256);
        CHECK_EQ(target.height(), 128);
        CHECK(target.byteSize() > 0);
    }
    GLFixture::release();
}

LUMINA_TEST(Gpu, poolReciclaAlvos) {
    if (!GLFixture::available()) return;
    CHECK(GLFixture::makeCurrent());
    GLFixture::gl().initialize();

    gpu::RenderTargetPool pool;
    pool.setMemoryBudget(64ull * 1024 * 1024);

    // Aquisitar, usar e devolver tem de devolver ao pool, e nao alocar de novo.
    const auto first = pool.acquire(128, 128);
    CHECK(first != nullptr);
    if (!first) { GLFixture::release(); return; }
    const size_t usedAfterAcquire = pool.usedBytes();

    pool.release(first);
    CHECK(first == nullptr);   // release limpa o ponteiro do chamador
    CHECK(pool.idleCount() == 1);

    const auto second = pool.acquire(128, 128);
    CHECK(second != nullptr);
    // O alvo veio do pool: a memoria nao cresceu.
    CHECK_EQ(pool.usedBytes(), usedAfterAcquire);
    CHECK_EQ(pool.idleCount(), size_t(0));

    pool.release(second);
    pool.clear();
    CHECK_EQ(pool.usedBytes(), size_t(0));
    GLFixture::release();
}

LUMINA_TEST(Gpu, poolRespeitaOrcamento) {
    if (!GLFixture::available()) return;
    CHECK(GLFixture::makeCurrent());
    GLFixture::gl().initialize();

    gpu::RenderTargetPool pool;
    // Orcamento minusculo: o pool tem de descartar o que nao cabe.
    pool.setMemoryBudget(32ull * 1024 * 1024);

    std::vector<gpu::RenderTargetPtr> held;
    for (int i = 0; i < 40; ++i) {
        held.push_back(pool.acquire(1024, 1024));
    }
    for (auto& t : held) pool.release(t);

    // Com 40 alvos de 8 MB cada, o orcamento de 32 MB so deixa poucos ociosos.
    CHECK(pool.usedBytes() <= 40ull * 1024 * 1024);
    pool.clear();
    GLFixture::release();
}

LUMINA_TEST(Gpu, renderizaGrafoSimples) {
    if (!GLFixture::available()) return;
    nodes::registerAllNodes();

    CHECK(GLFixture::makeCurrent());
    GLFixture::gl().initialize();

    Project project;
    project.createDefaultComposition();
    Composition* comp = project.composition(project.compositions().begin()->first);
    CHECK(comp != nullptr);
    if (!comp) { GLFixture::release(); return; }

    gpu::RenderTargetPool pool;
    pool.setMemoryBudget(128ull * 1024 * 1024);
    pool.setFormat(GLFixture::gl().caps().hasHalfFloat ? GL_RGBA16F : GL_RGBA8);

    gpu::Compositor compositor;
    compositor.setPool(&pool);

    gpu::RenderContext ctx(pool, 320, 180);
    gpu::RenderStats stats;

    const GLuint texture = compositor.render(*comp, 0.0, ctx, &project, &stats);
    CHECK(texture != 0);
    CHECK(!compositor.hasError());
    CHECK(stats.nodesEvaluated > 0);

    // O grafo padrao tem de produzir pixels, e nao um retangulo vazio.
    unsigned char pixel[4] = {0, 0, 0, 0};
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glBindTexture(GL_TEXTURE_2D, 0);

    CHECK(pixel[3] == 255);   // alpha opaco: o fundo e solido

    ctx.releaseAll();
    pool.clear();
    GLFixture::release();
}

LUMINA_TEST(Gpu, grafoVazioNaoQuebra) {
    if (!GLFixture::available()) return;
    nodes::registerAllNodes();

    CHECK(GLFixture::makeCurrent());
    GLFixture::gl().initialize();

    Project project;
    const CompId id = project.createComposition("Vazia", Size{64, 64}, 30.0, 1.0);

    gpu::RenderTargetPool pool;
    pool.setMemoryBudget(32ull * 1024 * 1024);

    gpu::Compositor compositor;
    compositor.setPool(&pool);

    gpu::RenderContext ctx(pool, 64, 64);
    // Um grafo vazio tem de renderizar o fundo, nao retornar nullptr: a
    // interface assume que sempre tem textura para mostrar.
    const GLuint texture = compositor.render(*project.composition(id), 0.0, ctx,
                                             &project, nullptr);
    CHECK(texture != 0);

    ctx.releaseAll();
    pool.clear();
    GLFixture::release();
}

LUMINA_TEST(Gpu, detectaRasterizadorPorSoftware) {
    if (!GLFixture::available()) return;
    CHECK(GLFixture::makeCurrent());
    GLFixture::gl().initialize();

    // A deteccao e o que permite avisar o usuario de que o modo leve faz
    // sentido. Ela nao pode falhar em nenhum driver.
    const auto& caps = GLFixture::gl().caps();
    CHECK(caps.maxTextureSize > 0);
    CHECK(!caps.glVersion.empty());
    CHECK(!caps.renderer.empty());

    GLFixture::release();
}
