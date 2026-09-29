#include "PlaybackController.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QCoreApplication>

namespace lmn::ui {
namespace {

// A precisao do relogio e melhor que o timer do Qt (que tem granularidade de
// ~16ms) e nao gera desperdicio: o visualizador decide quando renderizar.
constexpr int kTickIntervalMs = 8;

}  // namespace

PlaybackController::PlaybackController(Project& project, QObject* parent)
    : QObject(parent), m_project(project) {
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(kTickIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &PlaybackController::onTick);
}

PlaybackController::~PlaybackController() = default;

double PlaybackController::fps() const {
    return m_project.fps();
}

void PlaybackController::setFps(double value) {
    if (value > 0.0) m_project.setFps(value);
}

TimeRange PlaybackController::workArea() const {
    return m_project.timeline().range();
}

void PlaybackController::setWorkArea(TimeRange r) {
    m_project.timeline().setRange(r);
    emit workAreaChanged();
    setTime(clampTime(m_time));
}

Time PlaybackController::clampTime(Time t) const {
    const TimeRange r = workArea();
    if (r.duration() <= 0.0) return std::max(0.0, t);
    return std::clamp(t, r.in, r.out - 1e-6);
}

void PlaybackController::setTime(Time t) {
    const Time clamped = clampTime(t);
    if (std::abs(clamped - m_time) < 1e-9) return;
    m_time = clamped;
    m_frameRenderedSinceTick = false;
    emitTime();
}

void PlaybackController::emitTime() {
    emit timeChanged(m_time);
}

void PlaybackController::stepFrame(int delta) {
    const double dt = 1.0 / std::max(1.0, fps());
    if (m_state == State::Playing) pause();
    setTime(m_time + delta * dt);
}

void PlaybackController::goToStart() {
    setTime(workArea().in);
}

void PlaybackController::goToEnd() {
    setTime(workArea().out - 1e-6);
}

void PlaybackController::play() {
    if (m_state == State::Playing) return;
    m_state = State::Playing;
    m_lastTickTime = m_time;
    m_frameRenderedSinceTick = false;
    m_consecutiveDrops = 0;
    m_clock.restart();
    m_timer.start();
    emit stateChanged(static_cast<int>(m_state));
}

void PlaybackController::pause() {
    if (m_state == State::Paused || m_state == State::Stopped) return;
    m_state = State::Paused;
    m_timer.stop();
    emit stateChanged(static_cast<int>(m_state));
}

void PlaybackController::stop() {
    m_state = State::Stopped;
    m_timer.stop();
    setTime(workArea().in);
    emit stateChanged(static_cast<int>(m_state));
}

void PlaybackController::togglePlay() {
    if (m_state == State::Playing) {
        pause();
    } else {
        play();
    }
}

void PlaybackController::setLooping(bool on) {
    m_state = on ? State::Looping
                 : (m_state == State::Looping ? State::Playing : m_state);
    emit stateChanged(static_cast<int>(m_state));
}

void PlaybackController::onTick() {
    if (m_state != State::Playing && m_state != State::Looping) return;

    const qint64 elapsedNs = m_clock.nsecsElapsed();
    m_clock.restart();
    const double elapsed = elapsedNs / 1e9;

    // Se a maquina nao renderizou nada desde o ultimo tick, o relogio avancou
    // sem desenhar pixel algum. Contamos isso para avisar o usuario e
    // sugerir o modo leve.
    if (m_frameRenderedSinceTick) {
        m_consecutiveDrops = 0;
    } else {
        ++m_consecutiveDrops;
        if (m_dropFrames && m_consecutiveDrops >= 8) {
            emit droppingFrames(true);
        }
    }
    m_frameRenderedSinceTick = false;

    Time next = m_time + elapsed;

    const TimeRange r = workArea();
    if (r.duration() > 0.0 && next >= r.out) {
        if (m_state == State::Looping) {
            next = r.in;
            m_clock.restart();
        } else {
            next = r.out - 1e-6;
            pause();
        }
    }

    m_lastTickTime = next;
    setTime(next);
}

void PlaybackController::notifyFrameRendered(Time t) {
    m_frameRenderedSinceTick = true;
    emit frameRendered(t);
}

void PlaybackController::goToNextEditPoint() {
    const double dt = 1.0 / std::max(1.0, fps());
    double best = std::numeric_limits<double>::max();

    for (const auto& stack : m_project.timeline().stacks()) {
        for (const auto& clip : stack.clips()) {
            if (clip.start > m_time + 1e-6) best = std::min(best, clip.start);
            if (clip.end() > m_time + 1e-6) best = std::min(best, clip.end());
        }
    }

    // Tambem conta as keyframes da composicao em edicao.
    for (const auto& [cid, comp] : m_project.compositions()) {
        Q_UNUSED(cid);
        for (NodeId id : comp.graph().nodeIds()) {
            const Node* n = comp.graph().node(id);
            if (!n) continue;
            for (const auto& [name, prop] : n->properties()) {
                Q_UNUSED(name);
                for (const auto& key : prop.keyframes()) {
                    if (key.time > m_time + 1e-6) best = std::min(best, key.time);
                }
            }
        }
    }

    if (best < std::numeric_limits<double>::max()) {
        setTime(best);
    } else {
        setTime(m_time + dt);
    }
}

void PlaybackController::goToPreviousEditPoint() {
    const double dt = 1.0 / std::max(1.0, fps());
    double best = -std::numeric_limits<double>::max();

    for (const auto& stack : m_project.timeline().stacks()) {
        for (const auto& clip : stack.clips()) {
            if (clip.start < m_time - 1e-6) best = std::max(best, clip.start);
            if (clip.end() < m_time - 1e-6) best = std::max(best, clip.end());
        }
    }
    for (const auto& [cid, comp] : m_project.compositions()) {
        Q_UNUSED(cid);
        for (NodeId id : comp.graph().nodeIds()) {
            const Node* n = comp.graph().node(id);
            if (!n) continue;
            for (const auto& [name, prop] : n->properties()) {
                Q_UNUSED(name);
                for (const auto& key : prop.keyframes()) {
                    if (key.time < m_time - 1e-6) best = std::max(best, key.time);
                }
            }
        }
    }

    if (best > -std::numeric_limits<double>::max()) {
        setTime(best);
    } else {
        setTime(m_time - dt);
    }
}

void PlaybackController::setFrameCacheMemory(size_t bytes) {
    m_cacheBytes = std::max<size_t>(bytes, 8ull * 1024 * 1024);
    if (m_frameCacheUsed() < m_cacheBytes) return;
    // O cache real vive no visualizador; aqui so o limite. Se ja estourou,
    // pede a limpeza.
    clearFrameCache();
}

size_t PlaybackController::frameCacheUsed() const {
    return 0;   // preenchido pelo visualizador via frameCacheStats()
}

void PlaybackController::clearFrameCache() {
    // Sinal para quem tem o cache (o visualizador).
    emit stateChanged(static_cast<int>(m_state));
}

void PlaybackController::invalidateBefore(Time time) {
    Q_UNUSED(time);
    // A invalidacao real acontece no cache do visualizador, que observa a
    // revisao do grafo. Aqui registramos o ponto para o proximo render.
}

void PlaybackController::setQuality(int level) {
    m_quality = std::clamp(level, 0, 2);
    // Abaixo de 1 o cache deixa de fazer sentido: o custo de guardar o quadro
    // passa a ser maior que o de refaze-lo.
    if (m_quality < 1) m_cacheBytes = std::min<size_t>(m_cacheBytes, 24ull * 1024 * 1024);
}

}  // namespace lmn::ui
