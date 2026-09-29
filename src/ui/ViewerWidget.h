// ViewerWidget.h - Visualizador com renderizacao pela GPU.
//
// QOpenGLWidget porque da um contexto pronto e integra com o ciclo de pintura
// do Qt. O widget nao sabe nada sobre o grafo: ele pede um texture ao
// Compositor e desenha.
#pragma once

#include <memory>
#include <vector>

#include <QOpenGLWidget>
#include <QPointF>
#include <QRectF>

#include "core/Project.h"
#include "gpu/Compositor.h"
#include "gpu/GLContext.h"
#include "gpu/RenderTarget.h"
#include "ui/PlaybackController.h"

namespace lmn::media { class SourceProviderGL; }

namespace lmn::ui {

class ViewerWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    ViewerWidget(Project& project, PlaybackController& playback,
                 media::SourceProviderGL* sources, QWidget* parent = nullptr);
    ~ViewerWidget() override;

    // Qual composicao e o codigo da camera. O visualizador mostra o no
    // selecionado com o botao do meio (inspector de pixel).
    void setViewedComposition(CompId id);
    [[nodiscard]] CompId viewedComposition() const { return m_viewedComp; }

    void setViewedNode(NodeId id);
    [[nodiscard]] NodeId viewedNode() const { return m_viewedNode; }

    // Modo de visualizacao.
    void setShowCheckerboard(bool on) { m_checkerboard = on; invalidate(); }
    void setShowSafeAreas(bool on) { m_safeAreas = on; invalidate(); }
    void setSplitView(bool on) { m_splitView = on; invalidate(); }   // antes/depois
    void setSplitPosition(double t);   // 0..1, definido no .cpp
    void setOverlayEnabled(bool on) { m_overlay = on; invalidate(); }

    // Escala de renderizacao do preview. 0.5 metade da resolucao.
    void setPreviewScale(double scale);
    [[nodiscard]] double previewScale() const { return m_previewScale; }

    // Escala de exibicao (zoom / ajuste).
    void setZoom(double z);
    [[nodiscard]] double zoom() const { return m_zoom; }
    void zoomToFit();
    void zoomToActualSize();

    // Pixels. Serve para o conta-gotas e para o painel de cor.
    [[nodiscard]] QColor sampleColor(const QPoint& widgetPos) const;

    [[nodiscard]] QString rendererName() const;
    [[nodiscard]] bool isSoftwareRenderer() const;
    // Estatisticas do ultimo quadro, exibidas na barra de status.
    [[nodiscard]] QString lastRenderStats() const;

    // Forca o redesenho do proximo quadro (chamado quando o grafo muda).
    void invalidate();
    // Descarta o cache interno de frames ja renderizados.
    void clearCache();

signals:
    void colorPicked(QColor color);
    void renderError(QString message);
    // O modo leve foi ativado sozinho por queda de desempenho.
    void autoDowngraded(int qualityLevel);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    // Desenha a textura na tela, com o retangulo de destino ja calculado.
    void blit(GLuint texture, const QRectF& target);
    void drawCheckerboard(const QRectF& target);
    void drawSplitLine(const QRectF& target);
    void drawOverlays(const QRectF& imageRect);
    // Guarda o quadro final num alvo que sobrevive ao fim do frame, para o
    // conta-gotas poder ler depois.
    void keepForPicking(GLuint texture);
    [[nodiscard]] QRectF imageRect() const;
    [[nodiscard]] QPointF widgetToImage(const QPoint& pos) const;

    Project& m_project;
    PlaybackController& m_playback;
    media::SourceProviderGL* m_sources = nullptr;

    gpu::GLContext m_gl;
    gpu::Compositor m_compositor;
    gpu::RenderTargetPool m_pool;
    std::unique_ptr<gpu::RenderContext> m_context;

    CompId m_viewedComp = 0;
    NodeId m_viewedNode = kInvalidId;

    bool m_checkerboard = true;
    bool m_safeAreas = false;
    bool m_splitView = false;
    double m_splitPosition = 0.5;
    bool m_overlay = true;
    double m_previewScale = 1.0;
    double m_zoom = 1.0;
    bool m_fitToWindow = true;
    bool m_initialized = false;
    // Copia persistente do ultimo quadro. O RenderContext devolve todos os
    // alvos ao pool no fim do frame, entao o alvo final seria reciclado antes
    // do clique do usuario - e o conta-gotas leria o pixel errado. Esta copia
    // fica viva ate o proximo frame.
    gpu::RenderTargetPtr m_pickTarget;
    QString m_lastStats;
    QString m_lastError;
};

}  // namespace lmn::ui
