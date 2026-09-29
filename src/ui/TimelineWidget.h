// TimelineWidget.h - Linha do tempo multipista.
//
// Um unico widget pintado com QPainter. Cada pista e uma faixa; cada clip e um
// retangulo com nome, miniatura e marcadores de keyframe. Cliques com o botao
// direito e arrastar para mover sao o nucleo da edicao, entao receive
// prioridade sobre tudo mais.
#pragma once

#include <vector>

#include <QPointF>
#include <QRectF>
#include <QWidget>

#include "core/Project.h"
#include "ui/PlaybackController.h"
#include "ui/Theme.h"

namespace lmn::ui {

class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(Project& project, PlaybackController& playback,
                            QWidget* parent = nullptr);
    ~TimelineWidget() override;

    // --- Navegacao ------------------------------------------------------------
    [[nodiscard]] double pixelsPerSecond() const { return m_pps; }
    void zoom(double factor);
    void zoomToFit();
    void scrollToTime(Time t);

    // --- Selecao --------------------------------------------------------------
    void selectClip(ClipId id, bool additive = false);
    void clearSelection();
    [[nodiscard]] ClipId selectedClip() const { return m_selectedClip; }

    // --- Edicao ---------------------------------------------------------------
    // Corta o trecho do clip entre dois tempos da timeline.
    void splitAtPlayhead();
    void deleteSelection();
    void rippleDeleteSelection();
    void copySelection();
    void pasteAtPlayhead();
    void nudgeSelection(int frames);
    // Colapsa a duracao mantendo a origem: o "trim" de quem quer encurtar sem
    // perder o resto.
    void setSelectionDuration(Time duration);
    void setSelectionSpeed(double speed);

    // --- Visualizacao ---------------------------------------------------------
    void setShowThumbnails(bool on) { m_thumbnails = on; update(); }
    void setSnapEnabled(bool on) { m_snap = on; update(); }
    void setShowAudioWaveform(bool on) { m_waveforms = on; update(); }

    // Alturas automaticas: a pista cresce quando tem filho ou foco.
    void setAutoHeight(bool on) { m_autoHeight = on; layoutTracks(); }

signals:
    void selectionChanged();
    void clipDoubleClicked(ClipId id);
    void timelineChanged();
    void statusMessage(QString text);
    void contextMenuRequested(ClipId id, QPoint globalPos);
    void clipDragged(ClipId id, Time newStart);
    void tracksChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct TrackLayout {
        StackId id = 0;
        int top = 0;
        int height = 0;
        int collapsedHeight = 0;
        [[nodiscard]] QRectF headerRect() const;
        [[nodiscard]] QRectF laneRect() const;
    };

    void layoutTracks();
    [[nodiscard]] const TrackLayout* trackFor(StackId id) const;
    [[nodiscard]] const TrackLayout* trackAtY(int y) const;
    [[nodiscard]] Time xToTime(double x) const;
    [[nodiscard]] double timeToX(Time t) const;
    [[nodiscard]] QRectF clipRect(const Clip& clip, const TrackLayout& track) const;
    [[nodiscard]] ClipId clipAt(const QPointF& pos) const;
    [[nodiscard]] StackId stackAt(const QPointF& pos) const;
    [[nodiscard]] bool inHeader(const QPointF& pos) const;
    // Marcador de trim nas pontas do clip, com zona de pega.
    enum class ClipZone { None, Body, TrimIn, TrimOut };
    [[nodiscard]] ClipZone zoneFor(const QPointF& pos) const;
    [[nodiscard]] Time snapTime(Time t, ClipId ignore) const;
    void drawRuler(QPainter& p);
    void drawTracks(QPainter& p);
    void drawTrackHeaders(QPainter& p);
    void drawPlayhead(QPainter& p);
    void drawClip(QPainter& p, const Clip& clip, const TrackLayout& track);
    void drawWaveform(QPainter& p, const QRectF& rect, MediaId media);
    void drawKeyframeMarks(QPainter& p, const Clip& clip, const QRectF& rect);
    [[nodiscard]] int trackHeightFor(const Stack& stack) const;

    Project& m_project;
    PlaybackController& m_playback;
    Palette m_palette;
    Metrics m_metrics;

    double m_pps = 60.0;          // pixels por segundo
    double m_scrollTime = 0.0;    // tempo visivel no canto esquerdo
    std::vector<TrackLayout> m_tracks;
    ClipId m_selectedClip = 0;
    bool m_thumbnails = true;
    bool m_snap = true;
    bool m_waveforms = true;
    bool m_autoHeight = true;
    bool m_linkSelection = true;

    enum class Drag { None, Playhead, ClipMove, TrimIn, TrimOut, TrackHeight };
    Drag m_drag = Drag::None;
    QPointF m_dragStart;
    Time m_dragTimeStart = 0.0;
    Time m_dragClipOriginalStart = 0.0;
    Time m_dragClipOriginalIn = 0.0;
    Time m_dragClipOriginalOut = 0.0;
    ClipId m_dragClip = 0;
    StackId m_dragTrack = 0;
    double m_dragTrackHeight = 0.0;
    bool m_dragMoved = false;
};

}  // namespace lmn::ui
