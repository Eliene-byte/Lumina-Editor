#include "TimelineWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace lmn::ui {
namespace {

constexpr int kCollapsedHeight = 20;
constexpr int kMinTrackHeight = 22;
constexpr int kHeaderPadding = 4;

// Faixa de cores por tipo de pista: video, audio, efeitos.
void trackColor(const Stack& stack, QColor* fill, QColor* border) {
    const QString name = QString::fromStdString(stack.name());
    if (name.startsWith(QLatin1Char('A'))) {
        *fill = QColor(0x2c, 0x45, 0x38);
        *border = QColor(0x4a, 0x7a, 0x60);
    } else if (name.contains(QStringLiteral("fx"), Qt::CaseInsensitive)) {
        *fill = QColor(0x3d, 0x33, 0x2a);
        *border = QColor(0x7a, 0x5c, 0x36);
    } else {
        *fill = QColor(0x2a, 0x33, 0x44);
        *border = QColor(0x47, 0x5c, 0x7a);
    }
}

// Passos "redondos" para as marcas da regua: 1, 2, 5, 10, 15, 30, 60...
double niceStep(double rawSeconds) {
    const double candidates[] = {0.04, 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600};
    for (double c : candidates) {
        if (c >= rawSeconds) return c;
    }
    return 900.0;
}

QString formatTimecode(Time t, double fps) {
    const int totalFrames = static_cast<int>(std::llround(t * fps));
    const int f = totalFrames % std::max(1, static_cast<int>(fps));
    const int totalSeconds = totalFrames / std::max(1, static_cast<int>(fps));
    const int s = totalSeconds % 60;
    const int m = (totalSeconds / 60) % 60;
    const int h = totalSeconds / 3600;
    return QStringLiteral("%1:%2:%3:%4")
        .arg(h, 2, 10, QLatin1Char('0'))
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'))
        .arg(f, 2, 10, QLatin1Char('0'));
}

}  // namespace

QRectF TimelineWidget::TrackLayout::headerRect() const {
    return QRectF(0, top, Metrics::scaled(1.0).trackHeaderWidth, height);
}

QRectF TimelineWidget::TrackLayout::laneRect() const {
    const double w = Metrics::scaled(1.0).trackHeaderWidth;
    return QRectF(w, top, 100000, height);
}

TimelineWidget::TimelineWidget(Project& project, PlaybackController& playback,
                               QWidget* parent)
    : QWidget(parent), m_project(project), m_playback(playback) {
    m_palette = Palette::dark();
    m_metrics = Metrics::scaled(1.0);

    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, m_palette.surfaceAlt);
    setPalette(pal);
    setMinimumHeight(80);

    layoutTracks();
}

TimelineWidget::~TimelineWidget() = default;

int TimelineWidget::trackHeightFor(const Stack& stack) const {
    if (stack.empty()) return kCollapsedHeight;
    if (!m_autoHeight) return 46;
    // Uma pista com filho e com animacao precisa de espaco para os marcadores.
    const bool animated = stack.clips().size() > 1;
    return animated ? 44 : 34;
}

void TimelineWidget::layoutTracks() {
    m_tracks.clear();
    int y = m_metrics.rulerHeight;
    for (const auto& stack : m_project.timeline().stacks()) {
        TrackLayout t;
        t.id = stack.id();
        t.top = y;
        t.height = trackHeightFor(stack);
        t.collapsedHeight = kCollapsedHeight;
        m_tracks.push_back(t);
        y += t.height;
    }
    update();
}

void TimelineWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    layoutTracks();
}

// ---------------------------------------------------------------------------
// Coordenadas
// ---------------------------------------------------------------------------

double TimelineWidget::timeToX(Time t) const {
    const double lane = m_metrics.trackHeaderWidth;
    return lane + (t - m_scrollTime) * m_pps;
}

Time TimelineWidget::xToTime(double x) const {
    return m_scrollTime + (x - m_metrics.trackHeaderWidth) / m_pps;
}

QRectF TimelineWidget::clipRect(const Clip& clip, const TrackLayout& track) const {
    const double x = timeToX(clip.start);
    const double w = clip.duration() * m_pps;
    const double top = track.top + 3;
    return QRectF(x, top, std::max(2.0, w), track.height - 6);
}

const TimelineWidget::TrackLayout* TimelineWidget::trackFor(StackId id) const {
    for (const auto& t : m_tracks) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

const TimelineWidget::TrackLayout* TimelineWidget::trackAtY(int y) const {
    for (const auto& t : m_tracks) {
        if (y >= t.top && y < t.top + t.height) return &t;
    }
    return nullptr;
}

StackId TimelineWidget::stackAt(const QPointF& pos) const {
    const TrackLayout* t = trackAtY(static_cast<int>(pos.y()));
    return t ? t->id : 0;
}

bool TimelineWidget::inHeader(const QPointF& pos) const {
    return pos.x() < m_metrics.trackHeaderWidth;
}

ClipId TimelineWidget::clipAt(const QPointF& pos) const {
    const TrackLayout* t = trackAtY(static_cast<int>(pos.y()));
    if (!t || inHeader(pos)) return 0;
    const Stack* stack = m_project.timeline().findStack(t->id);
    if (!stack) return 0;

    // Do ultimo para o primeiro: o clip desenhado por cima ganha.
    const auto& clips = stack->clips();
    for (auto it = clips.rbegin(); it != clips.rend(); ++it) {
        if (clipRect(*it, *t).contains(pos)) return it->id;
    }
    return 0;
}

TimelineWidget::ClipZone TimelineWidget::zoneFor(const QPointF& pos) const {
    const ClipId id = clipAt(pos);
    if (!id) return ClipZone::None;

    const TrackLayout* t = trackAtY(static_cast<int>(pos.y()));
    const Stack* stack = t ? m_project.timeline().findStack(t->id) : nullptr;
    const Clip* clip = stack ? stack->findClip(id) : nullptr;
    if (!clip || !t) return ClipZone::None;

    const QRectF r = clipRect(*clip, *t);
    constexpr double grab = 7.0;
    if (pos.x() <= r.left() + grab) return ClipZone::TrimIn;
    if (pos.x() >= r.right() - grab) return ClipZone::TrimOut;
    return ClipZone::Body;
}

Time TimelineWidget::snapTime(Time t, ClipId ignore) const {
    if (!m_snap) return t;

    // Encaixe nas bordas dos outros clips e no cursor de reproducao. A
    // tolerancia e um pixel na tela, que e o que o usuario espera.
    const double tolerance = 8.0 / m_pps;
    Time best = t;
    double bestDist = tolerance;

    const auto consider = [&](Time candidate) {
        const double d = std::abs(candidate - t);
        if (d < bestDist) {
            bestDist = d;
            best = candidate;
        }
    };

    consider(m_playback.currentTime());
    for (const auto& stack : m_project.timeline().stacks()) {
        for (const auto& clip : stack.clips()) {
            if (clip.id == ignore) continue;
            consider(clip.start);
            consider(clip.end());
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Navegacao
// ---------------------------------------------------------------------------

void TimelineWidget::zoom(double factor) {
    const double before = xToTime(m_metrics.trackHeaderWidth + width() * 0.5);
    m_pps = std::clamp(m_pps * factor, 2.0, 4000.0);
    // Zoom ancorado no centro da visao.
    m_scrollTime = before - (width() * 0.5 - m_metrics.trackHeaderWidth) / m_pps;
    m_scrollTime = std::max(0.0, m_scrollTime);
    update();
}

void TimelineWidget::zoomToFit() {
    const TimeRange r = m_project.timeline().contentRange();
    if (r.duration() <= 0.0) {
        m_pps = 60.0;
        m_scrollTime = 0.0;
        update();
        return;
    }
    const double lane = width() - m_metrics.trackHeaderWidth - 20;
    m_pps = std::clamp(lane / r.duration(), 2.0, 4000.0);
    m_scrollTime = std::max(0.0, r.in - 0.5 / m_pps);
    update();
}

void TimelineWidget::scrollToTime(Time t) {
    const double lane = width() - m_metrics.trackHeaderWidth;
    const double visible = lane / m_pps;
    if (t < m_scrollTime) {
        m_scrollTime = std::max(0.0, t - 0.1);
    } else if (t > m_scrollTime + visible - 0.1) {
        m_scrollTime = t - visible * 0.5;
    }
    m_scrollTime = std::max(0.0, m_scrollTime);
    update();
}

// ---------------------------------------------------------------------------
// Selecao
// ---------------------------------------------------------------------------

void TimelineWidget::selectClip(ClipId id, bool additive) {
    m_selectedClip = id;
    Q_UNUSED(additive);
    update();
    emit selectionChanged();
}

void TimelineWidget::clearSelection() {
    if (!m_selectedClip) return;
    m_selectedClip = 0;
    update();
    emit selectionChanged();
}

// ---------------------------------------------------------------------------
// Pintura
// ---------------------------------------------------------------------------

void TimelineWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    p.fillRect(rect(), m_palette.surfaceAlt);
    drawRuler(p);
    drawTracks(p);
    drawTrackHeaders(p);
    drawPlayhead(p);
}

void TimelineWidget::drawRuler(QPainter& p) {
    const double lane = m_metrics.trackHeaderWidth;
    p.fillRect(0, 0, width(), m_metrics.rulerHeight, m_palette.surface);

    p.setFont(monoFont(8));
    p.setPen(QPen(m_palette.border, 1));
    p.drawLine(0, m_metrics.rulerHeight - 1, width(), m_metrics.rulerHeight - 1);

    // Escolhe o passo de forma que as marcas fiquem a ~80 px uma da outra.
    const double step = niceStep(80.0 / m_pps);
    const Time first = std::floor(m_scrollTime / step) * step;
    const Time last = xToTime(width());

    p.setPen(m_palette.textDim);
    for (Time t = first; t <= last; t += step) {
        const double x = timeToX(t);
        if (x < lane) continue;
        p.drawLine(QPointF(x, m_metrics.rulerHeight - 8), QPointF(x, m_metrics.rulerHeight - 1));
        p.drawText(QRectF(x + 3, 2, 90, m_metrics.rulerHeight - 10),
                   Qt::AlignLeft | Qt::AlignVCenter, formatTimecode(t, m_playback.fps()));
    }

    // Area de trabalho: o que sera exportado.
    const TimeRange wa = m_playback.workArea();
    const double x1 = timeToX(wa.in);
    const double x2 = timeToX(wa.out);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 60));
    if (x2 < width()) p.drawRect(QRectF(x2, 0, width() - x2, m_metrics.rulerHeight));
    if (x1 > lane) p.drawRect(QRectF(lane, 0, x1 - lane, m_metrics.rulerHeight));

    p.setPen(QPen(QColor(0x6c, 0xa0, 0x7a), 1));
    p.drawRect(QRectF(x1, 0, x2 - x1, m_metrics.rulerHeight - 1));
}

void TimelineWidget::drawTracks(QPainter& p) {
    for (const auto& t : m_tracks) {
        const Stack* stack = m_project.timeline().findStack(t.id);
        if (!stack) continue;

        QColor fill, border;
        trackColor(*stack, &fill, &border);
        if (!stack->visible()) fill.setAlpha(60);

        p.fillRect(t.laneRect(), fill);

        // Linha divisoria entre pistas
        p.setPen(QPen(m_palette.border, 1));
        p.drawLine(QPointF(t.laneRect().left(), t.top + t.height - 1),
                   QPointF(width(), t.top + t.height - 1));

        if (!stack->visible()) {
            p.setPen(m_palette.textDim);
            p.drawText(t.laneRect().adjusted(10, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                       QStringLiteral("pista oculta"));
            continue;
        }

        for (const auto& clip : stack->clips()) {
            drawClip(p, clip, t);
        }
    }
}

void TimelineWidget::drawClip(QPainter& p, const Clip& clip, const TrackLayout& track) {
    const QRectF r = clipRect(clip, track);
    if (r.right() < m_metrics.trackHeaderWidth || r.left() > width()) return;

    const bool selected = clip.id == m_selectedClip;
    const Stack* stack = m_project.timeline().findStack(track.id);
    QColor fill, border;
    trackColor(stack ? *stack : Stack(), &fill, &border);
    if (!clip.enabled) fill.setAlpha(50);

    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawRect(r);

    // Faixa de nome no topo
    p.setBrush(fill.lighter(130));
    p.drawRect(QRectF(r.left(), r.top(), r.width(), kHeaderPadding + 12));

    if (selected) {
        p.setPen(QPen(m_palette.accent, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r.adjusted(1, 1, -1, -1));
    } else {
        p.setPen(QPen(border, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r.adjusted(0, 0, -1, -1));
    }

    // Miniatura ou waveform na faixa util
    if (m_waveforms && stack && stack->name().rfind('A', 0) == 0) {
        drawWaveform(p, r, clip.media);
    } else if (m_thumbnails && r.height() > 26) {
        // Miniaturas reais vem do cache de quadros; sem ele, uma faixa
        // neutra e melhor do que cores inventadas.
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 30));
        p.drawRect(QRectF(r.left() + 1, r.top() + kHeaderPadding + 12,
                          r.width() - 2, r.height() - kHeaderPadding - 13));
    }

    drawKeyframeMarks(p, clip, r);

    // Nome
    if (r.width() > 24) {
        p.setPen(clip.enabled ? m_palette.text : m_palette.textDim);
        p.setFont(uiFont(0, false));
        const QRectF textRect(r.left() + 4, r.top() + 1, r.width() - 8,
                              kHeaderPadding + 11);
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                   QString::fromStdString(clip.name));

        // Duracao, se couber
        p.setFont(monoFont(7));
        p.setPen(m_palette.textDim);
        if (r.height() > 34) {
            p.drawText(QRectF(r.left() + 4, r.top() + kHeaderPadding + 12,
                              r.width() - 8, r.height() - kHeaderPadding - 12),
                       Qt::AlignLeft | Qt::AlignBottom | Qt::TextSingleLine,
                       formatTimecode(clip.duration(), m_playback.fps()));
        }
    }

    // Marcador de velocidade diferente de 1
    if (std::abs(clip.speed - 1.0) > 1e-3) {
        p.setPen(QPen(m_palette.warning, 1));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(r.right() - 7, r.top() + 6), 3, 3);
    }
}

void TimelineWidget::drawWaveform(QPainter& p, const QRectF& rect, MediaId media) {
    Q_UNUSED(media);
    // A forma de onda real vem do backend de audio. Sem ela, mostramos a
    // linha de base e anotamos isso, em vez de fingir que existe.
    const double midY = rect.center().y();
    p.setPen(QPen(m_palette.videoWaveform, 1));
    p.drawLine(QPointF(rect.left() + 2, midY), QPointF(rect.right() - 2, midY));
}

void TimelineWidget::drawKeyframeMarks(QPainter& p, const Clip& clip,
                                       const QRectF& rect) {
    if (rect.width() < 30) return;

    // Keyframes visiveis: as da composicao do clip, dentro do trecho.
    const Composition* comp = m_project.composition(clip.comp);
    if (!comp) return;

    const double y = rect.bottom() - 5;
    p.setBrush(m_palette.keyframe);
    p.setPen(Qt::NoPen);

    // Variaveis com animacao no grafo viram pontos de parada na timeline.
    for (NodeId id : comp->graph().nodeIds()) {
        const Node* n = comp->graph().node(id);
        if (!n) continue;
        for (const auto& [name, prop] : n->properties()) {
            Q_UNUSED(name);
            for (const auto& key : prop.keyframes()) {
                const Time t = clip.start + (key.time - clip.sourceIn) * clip.speed;
                if (t < clip.start - 1e-6 || t > clip.end() + 1e-6) continue;
                const double x = timeToX(t);
                if (x < rect.left() || x > rect.right()) continue;
                p.save();
                p.translate(x, y);
                p.rotate(45);
                p.drawRect(QRectF(-2.5, -2.5, 5, 5));
                p.restore();
            }
        }
    }
}

void TimelineWidget::drawTrackHeaders(QPainter& p) {
    for (const auto& t : m_tracks) {
        const Stack* stack = m_project.timeline().findStack(t.id);
        if (!stack) continue;

        p.fillRect(t.headerRect(), m_palette.surface);
        p.setPen(QPen(m_palette.border, 1));
        p.drawLine(QPointF(0, t.top + t.height - 1),
                   QPointF(m_metrics.trackHeaderWidth, t.top + t.height - 1));
        p.drawLine(QPointF(m_metrics.trackHeaderWidth, t.top),
                   QPointF(m_metrics.trackHeaderWidth, t.top + t.height));

        p.setPen(stack->visible() ? m_palette.text : m_palette.textDim);
        p.setFont(uiFont(0, false));
        p.drawText(QRectF(6, t.top, 90, t.height), Qt::AlignLeft | Qt::AlignVCenter,
                   QString::fromStdString(stack->name()));

        // Botoes: M (mudo), S (solo), olho (visivel), cadeado
        struct Btn { StackId id; const char* glyph; int x; bool on; };
        const double baseY = t.top + t.height - 8;
        double bx = m_metrics.trackHeaderWidth - 12;

        auto drawBtn = [&](const char* glyph, bool on, bool danger) {
            p.setPen(QPen(on ? (danger ? m_palette.danger : m_palette.warning)
                             : m_palette.textDim, 1));
            p.setFont(uiFont(7, on));
            p.drawText(QRectF(bx - 12, baseY - 10, 12, 12), Qt::AlignCenter,
                       QString::fromLatin1(glyph));
            bx -= 13;
        };
        drawBtn("M", stack->muted(), true);
        drawBtn("S", stack->solo(), false);
        drawBtn(stack->visible() ? "O" : "x", stack->visible(), false);
        drawBtn(stack->locked() ? "L" : "", stack->locked(), false);
    }
}

void TimelineWidget::drawPlayhead(QPainter& p) {
    const double x = timeToX(m_playback.currentTime());
    if (x < m_metrics.trackHeaderWidth) return;

    // Haste
    p.setPen(QPen(m_palette.playhead, 1));
    p.drawLine(QPointF(x, m_metrics.rulerHeight - 10), QPointF(x, height()));

    // Cabeca
    QPainterPath head;
    head.moveTo(x - 5, m_metrics.rulerHeight - 14);
    head.lineTo(x + 5, m_metrics.rulerHeight - 14);
    head.lineTo(x, m_metrics.rulerHeight - 4);
    head.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(m_palette.playhead);
    p.drawPath(head);
}

// ---------------------------------------------------------------------------
// Interacao
// ---------------------------------------------------------------------------

void TimelineWidget::mousePressEvent(QMouseEvent* event) {
    setFocus();
    const QPointF pos = event->position();
    m_dragStart = pos;
    m_dragTimeStart = xToTime(pos.x());
    m_dragMoved = false;

    // Botoes do cabecalho: M, S, olho, cadeado
    if (inHeader(pos)) {
        const TrackLayout* t = trackAtY(static_cast<int>(pos.y()));
        Stack* stack = t ? m_project.timeline().findStack(t->id) : nullptr;
        if (!stack) return;

        const double baseY = t->top + t->height - 8;
        const double bx = m_metrics.trackHeaderWidth - 12;
        if (pos.y() > baseY - 12) {
            const int slot = static_cast<int>((bx - pos.x()) / 13);
            switch (slot) {
                case 0: stack->setMuted(!stack->muted()); break;
                case 1: stack->setSolo(!stack->solo()); break;
                case 2: stack->setVisible(!stack->visible()); break;
                case 3: stack->setLocked(!stack->locked()); break;
                default: return;
            }
            emit tracksChanged();
            update();
            return;
        }

        // Clique em branco no cabecalho cria a proxima pista: e o atalho mais
        // usado depois de arrastar um clip para uma faixa nova.
        if (event->button() == Qt::LeftButton && pos.y() > t->top + 8) {
            m_drag = Drag::TrackHeight;
            m_dragTrack = t->id;
            m_dragTrackHeight = t->height;
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        if (pos.y() < m_metrics.rulerHeight) {
            m_drag = Drag::Playhead;
            m_playback.setTime(std::max(0.0, snapTime(m_dragTimeStart, 0)));
            return;
        }

        const ClipId id = clipAt(pos);
        if (id) {
            if (id != m_selectedClip) selectClip(id, event->modifiers() & Qt::ShiftModifier);

            Timeline::LocatedClip located = m_project.timeline().locate(id);
            const Clip* clip = located.clip;
            if (!clip) return;

            // Clique duplo ja e tratado em mouseDoubleClickEvent; aqui so
            // iniciamos o arraste correspondente.
            switch (zoneFor(pos)) {
                case ClipZone::TrimIn:
                    m_drag = Drag::TrimIn;
                    break;
                case ClipZone::TrimOut:
                    m_drag = Drag::TrimOut;
                    break;
                default:
                    m_drag = Drag::ClipMove;
                    break;
            }
            m_dragClip = id;
            m_dragTrack = located.stack ? located.stack->id() : 0;
            m_dragClipOriginalStart = clip->start;
            m_dragClipOriginalIn = clip->sourceIn;
            m_dragClipOriginalOut = clip->sourceOut;
            return;
        }

        clearSelection();
    }
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event) {
    const QPointF pos = event->position();

    // Cursor de arraste nas pontas do clip
    if (m_drag == Drag::None && !inHeader(pos) && pos.y() >= m_metrics.rulerHeight) {
        switch (zoneFor(pos)) {
            case ClipZone::TrimIn:
            case ClipZone::TrimOut:
                setCursor(Qt::SizeHorCursor);
                break;
            case ClipZone::Body:
                setCursor(Qt::OpenHandCursor);
                break;
            default:
                setCursor(Qt::ArrowCursor);
                break;
        }
        return;
    }

    if (m_drag == Drag::Playhead) {
        m_playback.setTime(std::max(0.0, xToTime(pos.x())));
        return;
    }

    if (m_drag == Drag::TrackHeight) {
        for (auto& t : m_tracks) {
            if (t.id != m_dragTrack) continue;
            t.height = static_cast<int>(std::clamp(
                m_dragTrackHeight + (pos.y() - m_dragStart.y()),
                static_cast<double>(kMinTrackHeight), 300.0));
        }
        update();
        return;
    }

    if (m_drag == Drag::None) return;

    Timeline::LocatedClip located = m_project.timeline().locate(m_dragClip);
    Clip* clip = located.clip;
    if (!clip) return;

    const double deltaSeconds = xToTime(pos.x()) - m_dragTimeStart;
    if (std::abs(pos.x() - m_dragStart.x()) > 2) m_dragMoved = true;

    switch (m_drag) {
        case Drag::ClipMove: {
            Time target = m_dragClipOriginalStart + deltaSeconds;
            target = std::max(0.0, snapTime(target, clip->id));
            clip->start = target;
            emit clipDragged(clip->id, target);
            break;
        }
        case Drag::TrimIn: {
            // Estica a entrada: a origem anda, a duracao muda, e o final
            // fica parado. E o comportamento esperado ao arrastar a borda
            // esquerda de um clip.
            const double dt = deltaSeconds;
            Time newIn = std::clamp(m_dragClipOriginalIn + dt, 0.0,
                                    m_dragClipOriginalOut - 0.01);
            clip->sourceIn = newIn;
            clip->start = m_dragClipOriginalStart + (newIn - m_dragClipOriginalIn);
            break;
        }
        case Drag::TrimOut: {
            Time newOut = std::max(m_dragClipOriginalIn + 0.01,
                                   m_dragClipOriginalOut + deltaSeconds);
            clip->sourceOut = newOut;
            break;
        }
        default:
            break;
    }
    update();
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    if (m_drag != Drag::None && m_drag != Drag::Playhead && m_dragMoved) {
        emit timelineChanged();
    } else if (m_drag == Drag::TrackHeight) {
        emit tracksChanged();
    }
    m_drag = Drag::None;
    m_dragClip = 0;
}

void TimelineWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    const ClipId id = clipAt(event->position());
    if (id) emit clipDoubleClicked(id);
}

void TimelineWidget::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        zoom(event->angleDelta().y() > 0 ? 1.2 : 1.0 / 1.2);
        event->accept();
        return;
    }
    // Rolagem horizontal com Shift, vertical com o resto.
    const double delta = event->angleDelta().x() != 0 ? event->angleDelta().x()
                                                      : event->angleDelta().y();
    m_scrollTime = std::max(0.0, m_scrollTime + delta / m_pps);
    update();
    event->accept();
}

void TimelineWidget::keyPressEvent(QKeyEvent* event) {
    const int frames = 1;
    switch (event->key()) {
        case Qt::Key_Space:
            m_playback.togglePlay();
            return;
        case Qt::Key_Left:
            m_playback.stepFrame(event->modifiers() & Qt::ShiftModifier ? -10 : -frames);
            return;
        case Qt::Key_Right:
            m_playback.stepFrame(event->modifiers() & Qt::ShiftModifier ? 10 : frames);
            return;
        case Qt::Key_Home:
            m_playback.goToStart();
            return;
        case Qt::Key_End:
            m_playback.goToEnd();
            return;
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            if (event->modifiers() & Qt::ShiftModifier) {
                rippleDeleteSelection();
            } else {
                deleteSelection();
            }
            return;
        case Qt::Key_I:
        case Qt::Key_O:
            // Atalhos de Premiere: I marca entrada, O marca saida.
            nudgeSelection(event->key() == Qt::Key_I ? -frames : frames);
            return;
        case Qt::Key_K:
            splitAtPlayhead();
            return;
        case Qt::Key_C:
            copySelection();
            return;
        case Qt::Key_V:
            pasteAtPlayhead();
            return;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoom(1.2);
            return;
        case Qt::Key_Minus:
            zoom(1.0 / 1.2);
            return;
        default:
            break;
    }
    QWidget::keyPressEvent(event);
}

void TimelineWidget::contextMenuEvent(QContextMenuEvent* event) {
    const ClipId id = clipAt(event->pos());
    if (id && id != m_selectedClip) selectClip(id);
    emit contextMenuRequested(id, event->globalPos());
}

// ---------------------------------------------------------------------------
// Edicao
// ---------------------------------------------------------------------------

void TimelineWidget::splitAtPlayhead() {
    Timeline::LocatedClip located = m_project.timeline().locate(m_selectedClip);
    Clip* clip = located.clip;
    Stack* stack = located.stack;
    if (!clip || !stack) return;

    const Time cut = m_playback.currentTime();
    if (cut <= clip->start + 0.01 || cut >= clip->end() - 0.01) {
        emit statusMessage(QStringLiteral("O cursor precisa estar dentro do clip"));
        return;
    }

    // O segundo pedaco precisa de id novo.
    Clip second = *clip;
    static ClipId nextId = 100000;
    second.id = nextId++;
    second.start = cut;
    second.sourceIn = clip->toSourceTime(cut);

    clip->sourceOut = clip->toSourceTime(cut);
    stack->clips().push_back(second);
    std::stable_sort(stack->clips().begin(), stack->clips().end(),
                     [](const Clip& a, const Clip& b) { return a.start < b.start; });

    selectClip(second.id);
    emit timelineChanged();
    layoutTracks();
    emit statusMessage(QStringLiteral("Clip dividido em %1")
                           .arg(formatTimecode(cut, m_playback.fps())));
}

void TimelineWidget::deleteSelection() {
    Timeline::LocatedClip located = m_project.timeline().locate(m_selectedClip);
    if (!located.clip || !located.stack) return;

    located.stack->removeClip(m_selectedClip);
    clearSelection();
    emit timelineChanged();
    layoutTracks();
}

void TimelineWidget::rippleDeleteSelection() {
    Timeline::LocatedClip located = m_project.timeline().locate(m_selectedClip);
    if (!located.clip || !located.stack) return;

    const Time removed = located.clip->duration();
    const Time at = located.clip->start;
    const StackId stackId = located.stack->id();

    located.stack->removeClip(m_selectedClip);
    // Ripple: tudo a frente anda para tras, fechando o buraco.
    for (auto& stack : m_project.timeline().stacks()) {
        if (stack.id() != stackId) continue;
        for (auto& clip : stack.clips()) {
            if (clip.start > at) clip.start -= removed;
        }
    }
    clearSelection();
    emit timelineChanged();
    layoutTracks();
    emit statusMessage(QStringLiteral("Removido e restante deslocado"));
}

void TimelineWidget::copySelection() {
    Timeline::LocatedClip located = m_project.timeline().locate(m_selectedClip);
    if (!located.clip) return;

    // Serializa o JSON do clip na area de transferencia: o mesmo formato do
    // arquivo de projeto, entao colar funciona tambem entre instancias.
    QJsonObject o;
    o["comp"] = static_cast<qint64>(located.clip->comp);
    o["name"] = QString::fromStdString(located.clip->name);
    o["in"] = located.clip->sourceIn;
    o["out"] = located.clip->sourceOut;
    o["speed"] = located.clip->speed;

    QApplication::clipboard()->setText(QString::fromUtf8(
        QJsonDocument(o).toJson(QJsonDocument::Compact)));
    emit statusMessage(QStringLiteral("Clip copiado"));
}

void TimelineWidget::pasteAtPlayhead() {
    const QString text = QApplication::clipboard()->text();
    if (text.isEmpty()) return;

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    const QJsonObject o = doc.object();
    Stack* stack = nullptr;
    for (auto& s : m_project.timeline().stacks()) {
        if (s.name().rfind('A', 0) != 0) { stack = &s; break; }
    }
    if (!stack) stack = m_project.timeline().addStack("V1");

    static ClipId nextId = 200000;
    Clip clip;
    clip.id = nextId++;
    clip.comp = static_cast<CompId>(o["comp"].toInt());
    clip.name = o["name"].toString().toStdString();
    clip.sourceIn = o["in"].toDouble(0.0);
    clip.sourceOut = o["out"].toDouble(clip.sourceIn + 1.0);
    clip.speed = o["speed"].toDouble(1.0);
    clip.start = m_playback.currentTime();

    stack->insert(clip);
    selectClip(clip.id);
    emit timelineChanged();
}

void TimelineWidget::nudgeSelection(int frames) {
    Timeline::LocatedClip located = m_project.timeline().locate(m_selectedClip);
    if (!located.clip) return;
    const double dt = frames / m_playback.fps();
    located.clip->start = std::max(0.0, located.clip->start + dt);
    update();
    emit timelineChanged();
}

void TimelineWidget::setSelectionDuration(Time duration) {
    Timeline::LocatedClip located = m_project.timeline().locate(m_selectedClip);
    if (!located.clip) return;
    located.clip->setDuration(std::max(0.01, duration));
    update();
    emit timelineChanged();
}

void TimelineWidget::setSelectionSpeed(double speed) {
    Timeline::LocatedClip located = m_project.timeline().locate(m_selectedClip);
    if (!located.clip) return;

    const double clamped = std::clamp(speed, 0.05, 20.0);
    const Time newDuration = located.clip->duration() * (located.clip->speed / clamped);
    located.clip->speed = clamped;
    located.clip->setDuration(newDuration);
    update();
    emit timelineChanged();
}

}  // namespace lmn::ui
