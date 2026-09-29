// PlaybackController.h - Relogio de reproducao, cache de frames e estado.
//
// Concentra o que os tres "consumidores" de tempo (visualizador, timeline,
// inspector) precisam concordar: onde estamos, se estamos tocando, e qual
// frame foi desenhado por ultimo. Sem isso, arrastar o cursor na timeline e
// clicar no visualizador dessincronizam em poucos segundos de uso.
#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <vector>

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include "core/Project.h"

namespace lmn::ui {

class PlaybackController : public QObject {
    Q_OBJECT
public:
    enum class State { Stopped, Playing, Paused, Looping, Stepping };

    explicit PlaybackController(Project& project, QObject* parent = nullptr);
    ~PlaybackController() override;

    // --- Tempo ----------------------------------------------------------------
    [[nodiscard]] Time currentTime() const { return m_time; }
    void setTime(Time t);
    // Move um frame, respeitando a direcao.
    void stepFrame(int delta);
    void goToStart();
    void goToEnd();
    void goToNextEditPoint();   // proximo clip ou keyframe
    void goToPreviousEditPoint();

    // --- Estado ---------------------------------------------------------------
    [[nodiscard]] State state() const { return m_state; }
    void play();
    void pause();
    void togglePlay();
    void stop();
    void setLooping(bool on);

    [[nodiscard]] double fps() const;
    void setFps(double fps);

    // --- Faixa de trabalho ----------------------------------------------------
    [[nodiscard]] TimeRange workArea() const;
    void setWorkArea(TimeRange r);

    // --- Sincronizacao com a timeline (clip em edicao) -----------------------
    void setSelectedStack(StackId id) { m_selectedStack = id; }
    [[nodiscard]] StackId selectedStack() const { return m_selectedStack; }

    // --- Cache ----------------------------------------------------------------
    // Espaco reservado para o cache de imagens. Em maquina fraca, o valor
    // padrao ja e baixo de proposito.
    void setFrameCacheMemory(size_t bytes);
    [[nodiscard]] size_t frameCacheMemory() const { return m_cacheBytes; }
    [[nodiscard]] size_t frameCacheUsed() const;
    void clearFrameCache();
    // Invalida o que foi renderizado antes de 'time' (editou um no la atras).
    void invalidateBefore(Time time);

    // --- Desempenho -----------------------------------------------------------
    void setQuality(int level);   // 0 = mais leve, 2 = melhor
    [[nodiscard]] int quality() const { return m_quality; }
    void setFrameDropEnabled(bool on) { m_dropFrames = on; }

    // --- Sinais ---------------------------------------------------------------
signals:
    void timeChanged(double time);
    void stateChanged(int state);
    void workAreaChanged();
    void frameRendered(double time);
    // Emitido quando um quadro atras do cursor: e o sinal para o modo leve
    // avisar que a maquina nao da conta.
    void droppingFrames(bool dropping);

public:
    // Chamado pelo visualizador depois de renderizar. Mantem o relogio em
    // dia com o que realmente foi mostrado.
    void notifyFrameRendered(Time t);

private slots:
    void onTick();

private:
    [[nodiscard]] Time clampTime(Time t) const;
    void emitTime();

    Project& m_project;
    Time m_time = 0.0;
    State m_state = State::Stopped;
    StackId m_selectedStack = 0;

    QTimer m_timer;
    QElapsedTimer m_clock;
    Time m_lastTickTime = 0.0;
    bool m_frameRenderedSinceTick = false;

    size_t m_cacheBytes = 96ull * 1024 * 1024;   // 96 MB
    int m_quality = 1;
    bool m_dropFrames = true;
    int m_consecutiveDrops = 0;
};

}  // namespace lmn::ui
