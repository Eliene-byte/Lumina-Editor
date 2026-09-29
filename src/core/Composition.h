// Composition.h - Uma composicao: tamanho, duracao e um grafo de nos.
#pragma once

#include <string>

#include "NodeGraph.h"
#include "Types.h"

namespace lmn {

// Espaco de cor em que o grafo e renderizado. O working space e sempre linear.
enum class WorkingSpace : uint8_t {
    Linear = 0,     // linear sRGB/Rec709, 1.0 = branco difuso
    Log100         // Cineon log (DaVinci)
};

enum class DisplayTransform : uint8_t {
    Standard = 0,  // sRGB com gamma
    Rec709 = 1,    // Rec.709
    P3 = 2,        // Display P3
    HLG = 3,        // Hybrid Log-Gamma
    ACES = 4,      // RRT approximated (ACEScc-like)
    Raw = 5,        // sem conversao, util para scopes
};

struct ColorManagementSettings {
    WorkingSpace working = WorkingSpace::Linear;
    DisplayTransform display = DisplayTransform::Rec709;
    double exposure = 0.0;      // stops
    double gamma = 1.0;
    bool autoGamutMap = true;
};

class Composition {
public:
    Composition() = default;
    Composition(CompId id, std::string name, const Size& size, double fps,
                Time duration);

    [[nodiscard]] CompId id() const noexcept { return m_id; }
    void setId(CompId id) { m_id = id; }

    [[nodiscard]] const std::string& name() const noexcept { return m_name; }
    void setName(std::string n) { m_name = std::move(n); }

    [[nodiscard]] const Size& size() const noexcept { return m_size; }
    void setSize(const Size& s) { m_size = s; }

    [[nodiscard]] double fps() const noexcept { return m_fps; }
    void setFps(double f) { m_fps = f > 0.0 ? f : 24.0; }

    [[nodiscard]] TimeRange duration() const noexcept { return m_duration; }
    void setDuration(TimeRange r) { m_duration = r; }

    // Area de trabalho: o intervalo shown quando a composicao abre.
    [[nodiscard]] TimeRange workArea() const noexcept { return m_workArea; }
    void setWorkArea(TimeRange r);

    [[nodiscard]] bool transparentBackground() const noexcept { return m_transparent; }
    void setTransparentBackground(bool t) { m_transparent = t; }

    [[nodiscard]] Color background() const noexcept { return m_background; }
    void setBackground(Color c) { m_background = c; }

    [[nodiscard]] NodeGraph& graph() noexcept { return m_graph; }
    [[nodiscard]] const NodeGraph& graph() const noexcept { return m_graph; }

    // --- Conversoes tempo <-> frame -------------------------------------------
    [[nodiscard]] Frame timeToFrame(Time t) const;
    [[nodiscard]] Time frameToTime(Frame f) const;
    [[nodiscard]] Frame durationInFrames() const;
    [[nodiscard]] Time frameDuration() const { return 1.0 / m_fps; }

    // --- Convenios ------------------------------------------------------------
    [[nodiscard]] Node* findNode(const std::string& type) const;
    [[nodiscard]] std::vector<CompId> children() const;  // composicoes usadas por algum no
    [[nodiscard]] bool isEmpty() const { return m_graph.empty(); }

private:
    CompId m_id = 0;
    std::string m_name = "Composition";
    Size m_size{1920, 1080};
    double m_fps = 30.0;
    TimeRange m_duration{0.0, 5.0};
    TimeRange m_workArea{0.0, 5.0};
    Color m_background{0.0, 0.0, 0.0, 1.0};
    bool m_transparent = false;
    NodeGraph m_graph;
};

}  // namespace lmn
