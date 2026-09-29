// Timeline.h - Pistas e clips: a linha do tempo no estilo Premiere/Resolve.
//
// Um clip aponta para uma Composition. Trocar de cena e trocar a referencia
// da composicao; transicoes vivem entre dois clips consecutivos da mesma pista.
#pragma once

#include <string>
#include <vector>

#include "Types.h"

namespace lmn {

class Composition;
class Project;

enum class TransitionType : uint8_t {
    None = 0,
    CrossDissolve,
    DipToBlack,
    DipToWhite,
    WipeLeft,
    WipeRight,
    Push,
    Slide,
    Zoom,
    Count,
};

const char* transitionTypeName(TransitionType t) noexcept;
TransitionType transitionTypeFromName(const std::string& s) noexcept;

struct Transition {
    TransitionType type = TransitionType::None;
    Time duration = 0.5;
    bool centered = true;   // metade antes / metade depois da emenda
    double amount = 1.0;    // 0..1, quanto da transicao e aplicado

    [[nodiscard]] bool isValid() const { return type != TransitionType::None && duration > 0.0; }
};

struct Clip {
    ClipId id = 0;
    CompId comp = 0;
    std::string name;
    MediaId media = 0;          // opcional: origem no pool de midia

    Time start = 0.0;           // posicao na timeline
    Time sourceIn = 0.0;        // ponto de entrada dentro da composicao de origem
    Time sourceOut = 0.0;       // ponto de saida
    double speed = 1.0;         // 1.0 = normal, 0.5 = camera lenta
    bool enabled = true;
    bool locked = false;
    double volume = 1.0;        // ganho de audio

    bool hasTransitionIn = false;
    bool hasTransitionOut = false;
    Transition transitionIn;
    Transition transitionOut;

    [[nodiscard]] Time duration() const {
        return std::max(0.0, (sourceOut - sourceIn) * speed);
    }
    [[nodiscard]] Time end() const { return start + duration(); }
    [[nodiscard]] TimeRange range() const { return TimeRange{start, end()}; }

    // Tempo da timeline -> tempo dentro da composicao de origem.
    [[nodiscard]] Time toSourceTime(Time timelineTime) const {
        if (speed <= kEpsilon) return sourceIn;
        return sourceIn + (timelineTime - start) / speed;
    }
    // Tempo da timeline considerando apenas o trecho visivel do clip.
    [[nodiscard]] bool contains(Time t) const { return t >= start && t < end(); }

    void setDuration(Time d);
};

class Stack {
public:
    Stack() = default;
    Stack(StackId id, std::string name, int index);

    [[nodiscard]] StackId id() const noexcept { return m_id; }
    [[nodiscard]] const std::string& name() const noexcept { return m_name; }
    void setName(std::string n) { m_name = std::move(n); }

    [[nodiscard]] int index() const noexcept { return m_index; }
    void setIndex(int i) { m_index = i; }

    [[nodiscard]] bool muted() const noexcept { return m_muted; }
    void setMuted(bool m) { m_muted = m; }
    [[nodiscard]] bool solo() const noexcept { return m_solo; }
    void setSolo(bool s) { m_solo = s; }
    [[nodiscard]] bool locked() const noexcept { return m_locked; }
    void setLocked(bool l) { m_locked = l; }
    [[nodiscard]] bool visible() const noexcept { return m_visible; }
    void setVisible(bool v) { m_visible = v; }
    [[nodiscard]] double opacity() const noexcept { return m_opacity; }
    void setOpacity(double o) { m_opacity = std::clamp(o, 0.0, 1.0); }

    [[nodiscard]] const std::vector<Clip>& clips() const noexcept { return m_clips; }
    std::vector<Clip>& clips() noexcept { return m_clips; }
    [[nodiscard]] bool empty() const noexcept { return m_clips.empty(); }
    [[nodiscard]] size_t size() const noexcept { return m_clips.size(); }

    Clip* findClip(ClipId id);
    const Clip* findClip(ClipId id) const;
    // Insere mantendo os clips ordenados por posicao.
    Clip* insert(Clip clip);
    bool removeClip(ClipId id);
    [[nodiscard]] TimeRange range() const;
    // Clips que se sobrepoem em t.
    [[nodiscard]] std::vector<const Clip*> clipsAt(Time t) const;

private:
    StackId m_id = 0;
    std::string m_name;
    int m_index = 0;
    bool m_muted = false;
    bool m_solo = false;
    bool m_locked = false;
    bool m_visible = true;
    double m_opacity = 1.0;
    std::vector<Clip> m_clips;
};

class Timeline {
public:
    [[nodiscard]] TimeRange range() const noexcept { return m_range; }
    void setRange(TimeRange r) { m_range = r; }

    [[nodiscard]] const std::vector<Stack>& stacks() const noexcept { return m_stacks; }
    std::vector<Stack>& stacks() noexcept { return m_stacks; }
    [[nodiscard]] size_t stackCount() const noexcept { return m_stacks.size(); }
    [[nodiscard]] bool empty() const noexcept { return m_stacks.empty(); }

    Stack* addStack(std::string name);
    bool removeStack(StackId id);
    [[nodiscard]] Stack* findStack(StackId id);
    [[nodiscard]] const Stack* findStack(StackId id) const;

    // Busca global do clip em qualquer pista.
    [[nodiscard]] Clip* findClip(ClipId id);
    struct LocatedClip { Stack* stack; Clip* clip; };
    [[nodiscard]] LocatedClip locate(ClipId id);
    void removeClipEverywhere(ClipId id);

    // Reorganiza os indices das pistas (V1, V2, A1...) apos reordenar na UI.
    void reindexStacks();
    [[nodiscard]] TimeRange contentRange() const;
    // Ajusta o tamanho da timeline para cobrir todos os clips.
    void fitToContent(double marginSeconds = 0.5);

private:
    TimeRange m_range{0.0, 10.0};
    std::vector<Stack> m_stacks;
};

}  // namespace lmn
