#include "ViewerWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>

#include "Theme.h"
#include "gpu/NodeUniforms.h"
#include "media/SourceProviderGL.h"

namespace lmn::ui {
namespace {

gpu::ShaderProgram* viewProgram(gpu::ShaderProgram* storage) {
    if (!storage->valid() && !storage->compileResource("viewer.blit",
                                                        ":/shaders/copy.frag")) {
        return nullptr;
    }
    return storage;
}

}  // namespace

ViewerWidget::ViewerWidget(Project& project, PlaybackController& playback,
                           media::SourceProviderGL* sources, QWidget* parent)
    : QOpenGLWidget(parent), m_project(project), m_playback(playback),
      m_sources(sources) {
    setMinimumSize(320, 180);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAutoFillBackground(false);

    const Palette p = Palette::dark();
    auto* pal = palette();
    pal.setColor(QPalette::Window, p.background);
    setPalette(pal);
}

ViewerWidget::~ViewerWidget() {
    makeCurrent();
    m_compositor.setSourceProvider(nullptr);
    m_context.reset();
    m_pickTarget.reset();
    m_pool.clear();
    if (m_sources) m_sources->invalidateAll();
    doneCurrent();
}

void ViewerWidget::setViewedComposition(CompId id) {
    if (m_viewedComp == id) return;
    m_viewedComp = id;
    invalidate();
}

void ViewerWidget::setViewedNode(NodeId id) {
    if (m_viewedNode == id) return;
    m_viewedNode = id;
    invalidate();
}

void ViewerWidget::setPreviewScale(double scale) {
    const double clamped = std::clamp(scale, 0.125, 1.0);
    if (std::abs(clamped - m_previewScale) < 1e-6) return;
    m_previewScale = clamped;
    clearCache();
    invalidate();
}

void ViewerWidget::setZoom(double z) {
    m_zoom = std::clamp(z, 0.05, 16.0);
    m_fitToWindow = false;
    invalidate();
}

void ViewerWidget::zoomToFit() {
    m_fitToWindow = true;
    m_zoom = 1.0;
    invalidate();
}

void ViewerWidget::zoomToActualSize() {
    m_fitToWindow = false;
    m_zoom = 1.0;
    invalidate();
}

void ViewerWidget::invalidate() {
    update();
}

void ViewerWidget::clearCache() {
    makeCurrent();
    m_pool.clear();
    m_compositor.invalidateCaches();
    doneCurrent();
}

void ViewerWidget::initializeGL() {
    m_gl.initialize();

    m_pool.setFormat(m_gl.caps().hasHalfFloat ? GL_RGBA16F : GL_RGBA8);
    // 128 MB de video: sobra para varios nos mesmo em um PC com 4 GB de RAM.
    m_pool.setMemoryBudget(128ull * 1024 * 1024);

    m_compositor.setPool(&m_pool);
    m_compositor.setSourceProvider(m_sources);
    m_compositor.setDisplayTransform(m_project.color());
    m_compositor.setPreviewScale(m_previewScale);

    // Rasterizacao por software e a situacao mais comum em "computador
    // modesto": avisamos e deixamos o modo leve disponivel.
    if (m_gl.caps().isSoftware) {
        setPreviewScale(0.5);
        m_pool.setMemoryBudget(48ull * 1024 * 1024);
    }

    m_initialized = true;
}

void ViewerWidget::resizeGL(int w, int h) {
    Q_UNUSED(w);
    Q_UNUSED(h);
    m_context.reset();
}

void ViewerWidget::paintGL() {
    if (!m_initialized) {
        glClearColor(0.06f, 0.06f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    const Composition* comp = m_project.composition(m_viewedComp);
    if (!comp) {
        m_compositor.clearError();
        glClearColor(0.06f, 0.06f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    // Resolucao efetiva: a composicao reduzida pela escala de preview, presa
    // ao maximo de textura da GPU. Um PC modesto pode ter 2048 de limite.
    const int maxTex = m_gl.caps().maxTextureSize;
    int w = std::max(1, static_cast<int>(comp->size().width * m_previewScale));
    int h = std::max(1, static_cast<int>(comp->size().height * m_previewScale));
    w = std::min(w, maxTex);
    h = std::min(h, maxTex);

    if (!m_context || m_context->width() != w || m_context->height() != h) {
        m_context = std::make_unique<gpu::RenderContext>(m_pool, w, h);
    }

    // Nao ha VAO para este contexto ainda se a janela foi recriada.
    gpu::RenderStats stats;
    const GLuint texture = m_compositor.render(
        const_cast<Composition&>(*comp), m_playback.currentTime(), *m_context,
        &m_project, &stats);

    m_compositor.clearError();
    if (!m_compositor.hasError() && texture) {
        m_lastStats = QStringLiteral("%1 nos, %2 ms")
                          .arg(stats.nodesEvaluated)
                          .arg(stats.milliseconds, 0, 'f', 1);
    }

    const QRectF target = imageRect();

    if (m_checkerboard) drawCheckerboard(target);

    if (texture) {
        blit(texture, target);
        keepForPicking(texture);
    }

    if (m_splitView) drawSplitLine(target);
    if (m_safeAreas) drawOverlays(target);

    m_context->releaseAll();
    m_playback.notifyFrameRendered(m_playback.currentTime());
}

void ViewerWidget::keepForPicking(GLuint texture) {
    if (!m_pickTarget) {
        m_pickTarget = std::make_unique<gpu::RenderTarget>();
        m_pickTarget->create(1, 1, GL_RGBA8, false);
    }
    if (!m_pickTarget->valid()) return;

    // Copia o alvo final para um alvo proprio, com o mesmo tamanho, que
    // sobrevive ao fim do frame. Sem esta copia o conta-gotas leria lixo.
    if (m_pickTarget->width() != m_context->width() ||
        m_pickTarget->height() != m_context->height()) {
        m_pickTarget->create(m_context->width(), m_context->height(), m_pool.format(),
                             false);
        if (!m_pickTarget->valid()) return;
    }

    gpu::ShaderProgram copy;
    if (!copy.compileResource("viewer.copy", ":/shaders/copy.frag")) return;

    m_pickTarget->bindAsTarget();
    copy.bind();
    copy.bindTexture("u_input", 0, texture);
    copy.setInt("u_hasInput", 1);
    copy.setInt("u_useTransform", 0);
    copy.setFloat("u_opacity", 1.0f);
    copy.setInt("u_premultiply", 0);
    copy.setInt("u_straighten", 0);   // ja esta premultiplicado
    gpu::fullscreenTriangle();
}

void ViewerWidget::drawCheckerboard(const QRectF& target) {
    gpu::RenderTargetPtr bg = m_context->acquire();
    if (!bg) return;

    gpu::ShaderProgram checker;
    if (!checker.compileResource("viewer.checker", ":/shaders/checkerboard.frag")) {
        return;
    }

    bg->bindAsTarget();
    checker.bind();
    checker.setVec2("u_texel", Vec2{1.0 / bg->width(), 1.0 / bg->height()});
    checker.setColor("u_colorA", Color{0.16, 0.16, 0.16, 1.0});
    checker.setColor("u_colorB", Color{0.22, 0.22, 0.22, 1.0});
    checker.setInt("u_squares", 12);
    gpu::fullscreenTriangle();

    blit(bg->texture(), target);
}

void ViewerWidget::drawSplitLine(const QRectF& target) {
    QPainter painter(this);
    painter.setPen(QPen(Palette::dark().textInverted, 1));
    const double x = target.left() + target.width() * m_splitPosition;
    painter.drawLine(QPointF(x, target.top()), QPointF(x, target.bottom()));
    painter.end();
}

void ViewerWidget::blit(GLuint texture, const QRectF& target) {
    static gpu::ShaderProgram program;
    gpu::ShaderProgram* p = viewProgram(&program);
    if (!p) return;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width() * devicePixelRatioF(), height() * devicePixelRatioF());
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // A conversao linear -> tela acontece no no color.viewTransform do
    // grafo, e nao aqui. Sem esse no, a imagem sai escura - que e o
    // comportamento correto: o usuario ve que falta a etapa final em vez de
    // receber um resultado silenciosamente errado.
    p->bind();
    p->bindTexture("u_input", 0, texture);
    p->setInt("u_hasInput", texture ? 1 : 0);
    p->setFloat("u_opacity", 1.0f);
    p->setInt("u_useTransform", 0);
    p->setFloat("u_flipX", 0.0f);
    p->setFloat("u_flipY", 0.0f);
    p->setInt("u_premultiply", 0);
    p->setInt("u_straighten", 1);   // a textura esta premultiplicada; a tela nao

    // Desenha so na regiao da imagem.
    glEnable(GL_SCISSOR_TEST);
    const QRect r = target.toAlignedRect();
    glScissor(r.x() * devicePixelRatioF(), r.y() * devicePixelRatioF(),
              r.width() * devicePixelRatioF(), r.height() * devicePixelRatioF());
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisable(GL_SCISSOR_TEST);
}

QRectF ViewerWidget::imageRect() const {
    const Composition* comp = m_project.composition(m_viewedComp);
    if (!comp || !comp->size().isValid()) return QRectF(0, 0, width(), height());

    const double aspect = comp->size().aspect();
    const double widgetAspect = static_cast<double>(width()) / std::max(1, height());

    double w, h;
    if (m_fitToWindow) {
        // "Caber" como padrao: nada e cortado, que e o que se espera de um
        // editor de video.
        if (widgetAspect > aspect) {
            h = height();
            w = h * aspect;
        } else {
            w = width();
            h = w / aspect;
        }
    } else {
        w = comp->size().width * m_zoom;
        h = comp->size().height * m_zoom;
    }

    return QRectF((width() - w) * 0.5, (height() - h) * 0.5, w, h);
}

QPointF ViewerWidget::widgetToImage(const QPoint& pos) const {
    const QRectF r = imageRect();
    if (r.width() <= 0.0 || r.height() <= 0.0) return {};
    const double u = (pos.x() - r.x()) / r.width();
    const double v = 1.0 - (pos.y() - r.y()) / r.height();   // y para cima
    return {u, v};
}

QColor ViewerWidget::sampleColor(const QPoint& widgetPos) const {
    // Ler a GPU a cada movimento do mouse travaria a interface, entao lemos
    // uma unica vez, no clique, sobre a copia persistente do ultimo quadro.
    const QPointF uv = widgetToImage(widgetPos);
    if (uv.x() < 0.0 || uv.x() > 1.0 || uv.y() < 0.0 || uv.y() > 1.0) return {};
    if (!m_pickTarget || !m_pickTarget->valid()) return {};

    makeCurrent();

    const int w = m_pickTarget->width();
    const int h = m_pickTarget->height();
    const int x = std::clamp(static_cast<int>(uv.x * (w - 1)), 0, w - 1);
    const int y = std::clamp(static_cast<int>((1.0 - uv.y) * (h - 1)), 0, h - 1);

    // glReadPixels tem origem no canto inferior esquerdo; y ja vem invertido.
    unsigned char pixel[4] = {0, 0, 0, 0};
    glBindFramebuffer(GL_FRAMEBUFFER, m_pickTarget->fbo());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    doneCurrent();

    return QColor(pixel[0], pixel[1], pixel[2], pixel[3]);
}

void ViewerWidget::drawOverlays(const QRectF& imageRect) {
    // Desenhado com QPainter por cima do GL: e um overlay de 1 pixel de
    // espessura, nao vale o custo de um shader.
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const Palette p = Palette::dark();

    // Regra dos tercos.
    painter.setPen(QPen(QColor(255, 255, 255, 60), 1, Qt::DashLine));
    for (int i = 1; i < 3; ++i) {
        const double x = imageRect.left() + imageRect.width() * i / 3.0;
        const double y = imageRect.top() + imageRect.height() * i / 3.0;
        painter.drawLine(QPointF(x, imageRect.top()), QPointF(x, imageRect.bottom()));
        painter.drawLine(QPointF(imageRect.left(), y), QPointF(imageRect.right(), y));
    }

    // Area segura de titulos (90%) e de acao (80%).
    painter.setPen(QPen(QColor(255, 255, 255, 110), 1, Qt::SolidLine));
    const QRectF title = imageRect.adjusted(imageRect.width() * 0.05,
                                            imageRect.height() * 0.05,
                                            -imageRect.width() * 0.05,
                                            -imageRect.height() * 0.05);
    const QRectF action = imageRect.adjusted(imageRect.width() * 0.10,
                                             imageRect.height() * 0.10,
                                             -imageRect.width() * 0.10,
                                             -imageRect.height() * 0.10);
    painter.drawRect(title);
    painter.drawRect(action);

    // Borda da imagem, para separar do fundo.
    painter.setPen(QPen(p.border, 1));
    painter.drawRect(imageRect.adjusted(0, 0, -1, -1));
    painter.end();
}

void ViewerWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        const QColor c = sampleColor(event->pos());
        if (c.isValid()) {
            emit colorPicked(c);
            setCursor(Qt::CrossCursor);
        }
        return;
    }
    QOpenGLWidget::mousePressEvent(event);
}

void ViewerWidget::mouseMoveEvent(QMouseEvent* event) {
    setToolTip(QStringLiteral("x: %1  y: %2")
                   .arg(widgetToImage(event->pos()).x(), 0, 'f', 3)
                   .arg(widgetToImage(event->pos()).y(), 0, 'f', 3));
    QOpenGLWidget::mouseMoveEvent(event);
}

void ViewerWidget::wheelEvent(QWheelEvent* event) {
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    if (event->modifiers() & Qt::ControlModifier) {
        setZoom(m_zoom * factor);
        event->accept();
        return;
    }
    // Sem Ctrl: desloca o cursor, que e o comportamento de um visualizador de
    // video profissional.
    m_fitToWindow = false;
    m_zoom = std::clamp(m_zoom * factor, 0.05, 16.0);
    update();
    event->accept();
}

QString ViewerWidget::rendererName() const {
    return m_gl.rendererString();
}

bool ViewerWidget::isSoftwareRenderer() const {
    return m_gl.caps().isSoftware;
}

QString ViewerWidget::lastRenderStats() const {
    return m_lastStats;
}

}  // namespace lmn::ui
