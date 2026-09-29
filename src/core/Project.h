// Project.h - Raiz de tudo: o arquivo .lumina.
//
// O Project e a unica coisa que a UI manipula em bloco; composicoes, timeline
// e acervo sao colunas dele. Ids sao globais e monotonicos para que um no
// possa referenciar uma composicao sem ambiguidade.
#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "Composition.h"
#include "MediaPool.h"
#include "Timeline.h"

namespace lmn {

struct OutputSettings {
    Size size{1920, 1080};
    double fps = 30.0;
    std::string container = "mp4";
    std::string videoCodec = "h264";
    std::string audioCodec = "aac";
    std::string preset = "medium";
    int videoBitrateKbps = 12000;
    int audioBitrateKbps = 192;
    bool withAudio = true;
    bool alphaChannel = false;
    bool useProxy = false;   // exportar usando os proxies (render mais rapido)
    [[nodiscard]] TimeRange range() const { return rangeOverride; }
    TimeRange rangeOverride{0.0, 0.0};
    [[nodiscard]] bool rangeIsAuto() const { return rangeOverride.duration() <= 0.0; }
};

// Granularidade de desfazer. Cada comando da UI vira um UndoCommand.
class UndoStack;

class Project {
public:
    static constexpr int kFileVersion = 1;

    Project();

    // --- Identidade -----------------------------------------------------------
    [[nodiscard]] const std::string& name() const noexcept { return m_name; }
    void setName(std::string n) { m_name = std::move(n); }
    [[nodiscard]] const std::string& filePath() const noexcept { return m_filePath; }
    void setFilePath(std::string p) { m_filePath = std::move(p); }
    [[nodiscard]] bool isDirty() const noexcept { return m_dirty; }
    void markClean() { m_dirty = false; }
    void markDirty() { m_dirty = true; }

    // --- Composicoes ----------------------------------------------------------
    [[nodiscard]] const std::map<CompId, Composition>& compositions() const noexcept {
        return m_comps;
    }
    [[nodiscard]] std::map<CompId, Composition>& compositions() noexcept { return m_comps; }
    [[nodiscard]] Composition* composition(CompId id);
    [[nodiscard]] const Composition* composition(CompId id) const;
    [[nodiscard]] size_t compositionCount() const noexcept { return m_comps.size(); }

    CompId createComposition(std::string name, const Size& size, double fps, Time duration);
    bool removeComposition(CompId id);
    // Copia uma composicao e tudo que ela referencia, recriando os ids.
    CompId duplicateComposition(CompId id, const std::string& newName = {});
    // Substitui todas as referencias a 'from' por 'to'.
    void remapComposition(CompId from, CompId to);
    [[nodiscard]] std::vector<CompId> compositionsUsedBy(CompId id) const;
    [[nodiscard]] bool compositionIsUsed(CompId id) const;

    // --- Linha do tempo e acervo ----------------------------------------------
    [[nodiscard]] Timeline& timeline() noexcept { return m_timeline; }
    [[nodiscard]] const Timeline& timeline() const noexcept { return m_timeline; }
    [[nodiscard]] MediaPool& media() noexcept { return m_media; }
    [[nodiscard]] const MediaPool& media() const noexcept { return m_media; }

    // --- Formato do projeto ---------------------------------------------------
    [[nodiscard]] const Size& size() const noexcept { return m_size; }
    void setSize(const Size& s);
    [[nodiscard]] double fps() const noexcept { return m_fps; }
    void setFps(double f);

    [[nodiscard]] ColorManagementSettings& color() noexcept { return m_color; }
    [[nodiscard]] const ColorManagementSettings& color() const noexcept { return m_color; }

    [[nodiscard]] OutputSettings& output() noexcept { return m_output; }
    [[nodiscard]] const OutputSettings& output() const noexcept { return m_output; }

    // --- Tempo ----------------------------------------------------------------
    [[nodiscard]] Frame timeToFrame(Time t) const;
    [[nodiscard]] Time frameToTime(Frame f) const;

    // --- Revisões -------------------------------------------------------------
    // Qualquer mudanca estrutural incrementa a revisao; a UI usa isso para
    // invalidar caches de render e miniaturas sem diffing o grafo inteiro.
    [[nodiscard]] uint64_t revision() const noexcept { return m_revision; }
    void touch() { ++m_revision; m_dirty = true; fireChanged(); }
    using ChangeCallback = std::function<void()>;
    void setChangeCallback(ChangeCallback cb) { m_onChange = std::move(cb); }

    // --- Edicao ---------------------------------------------------------------
    void clear();
    // Grafo minimo de boas-vindas: um Solid preto, um Merge e um View.
    void createDefaultComposition();

private:
    void fireChanged() { if (m_onChange) m_onChange(); }

    std::string m_name = "Projeto sem titulo";
    std::string m_filePath;
    Size m_size{1920, 1080};
    double m_fps = 30.0;
    bool m_dirty = false;
    uint64_t m_revision = 1;
    std::map<CompId, Composition> m_comps;
    Timeline m_timeline;
    MediaPool m_media;
    ColorManagementSettings m_color;
    OutputSettings m_output;
    CompId m_nextCompId = 1;
    ChangeCallback m_onChange;
};

}  // namespace lmn
