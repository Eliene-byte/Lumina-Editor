// Node.h - Um no do grafo de composicao.
//
// Um no e simultaneamente uma "camada" e um "operador": um Solid gerador tem
// 0 entradas e 1 saida; um Merge tem 2 entradas; um Timewarp tem 0 entradas.
// O grafo resultante cobre o modelo de camadas do After Effects e o modelo de
// nos do Fusion/Resolve com uma unica estrutura.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "BlendMode.h"
#include "Property.h"
#include "Types.h"

namespace lmn {

// Matriz 3x3 row-major para transformacoes 2D/3D sem projecao.
struct Transform {
    double m[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};

    static Transform identity() { return {}; }
    static Transform translation(const Vec2& t);
    static Transform scale(const Vec2& s);
    static Transform rotationZ(double radians);

    [[nodiscard]] Transform operator*(const Transform& o) const;
    [[nodiscard]] Vec2 applyPoint(const Vec2& p) const;
    [[nodiscard]] Vec2 applyVector(const Vec2& v) const;
    [[nodiscard]] Vec2 applyInversePoint(const Vec2& p) const;
    [[nodiscard]] double determinant() const;
    [[nodiscard]] bool invert(Transform& out) const;
    [[nodiscard]] bool hasSkew() const;

    // Decomposicao aproximada, usada pelo editor de grafico para a ellipse de
    // transformacao e pelo snap de valores.
    [[nodiscard]] Vec2 translationPart() const;
    [[nodiscard]] Vec2 scalePart() const;
    [[nodiscard]] double rotationZ() const;
};

// Cache: quanto o resultado do no pode ser reutilizado entre frames.
enum class CachePolicy : uint8_t {
    Never = 0,   // sempre recalcula
    Frame,       // por frame
    Region,      // por regiao retangular
    Full,        // so invalida quando uma entrada muda
    Count,
};

const char* cachePolicyName(CachePolicy c) noexcept;
CachePolicy cachePolicyFromName(const std::string& s) noexcept;

enum class NodeFlag : uint32_t {
    None = 0,
    Solo = 1u << 0,          // isola este no no preview
    Locked = 1u << 1,        // trava posicao e conexoes na UI
    ThreeD = 1u << 2,        // participa do depth buffer 3D
    PassThrough = 1u << 3,   // repassa a entrada sem alterar nada
    MotionBlur = 1u << 4,    // habilita amostragem temporal
    Transparent = 1u << 5,   // a composicao aceita alpha
    Collapsed = 1u << 6,     // UI: cabecalho recolhido
    Selected = 1u << 7,      // UI
    Seamless = 1u << 8,      // o no repete ciclicamente ao longo do tempo
};

inline constexpr NodeFlag operator|(NodeFlag a, NodeFlag b) {
    return static_cast<NodeFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline constexpr NodeFlag operator&(NodeFlag a, NodeFlag b) {
    return static_cast<NodeFlag>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline constexpr NodeFlag& operator|=(NodeFlag& a, NodeFlag b) { a = a | b; return a; }
inline constexpr bool hasFlag(NodeFlag v, NodeFlag f) {
    return (static_cast<uint32_t>(v) & static_cast<uint32_t>(f)) != 0;
}

class Node {
public:
    Node() = default;
    Node(NodeId id, std::string type, std::string name);

    // --- Identidade -----------------------------------------------------------
    [[nodiscard]] NodeId id() const noexcept { return m_id; }
    [[nodiscard]] const std::string& type() const noexcept { return m_type; }
    // Copia tudo de outro no, com id novo. Propriedades, conexoes de entrada
    // e estado sao preservados: a posicao no grafo e deslocada pelo chamador.
    [[nodiscard]] Node cloned(NodeId newId) const;
    // Copia apenas os valores das propriedades, mantendo as chaves de animacao
    // de um no de referencia. Usado ao colar em outro no.
    void copyPropertyValuesFrom(const Node& other);

    [[nodiscard]] const std::string& name() const noexcept { return m_name; }
    void setName(std::string n) { m_name = std::move(n); }
    // Nome exibido: o nome customizado ou o id, para rotulos no grafo.
    [[nodiscard]] std::string displayName() const;

    // --- Estado ---------------------------------------------------------------
    [[nodiscard]] bool isEnabled() const noexcept { return m_enabled; }
    void setEnabled(bool e) { m_enabled = e; }

    [[nodiscard]] NodeFlag flags() const noexcept { return m_flags; }
    void setFlag(NodeFlag f, bool on = true);
    [[nodiscard]] bool flag(NodeFlag f) const noexcept { return hasFlag(m_flags, f); }

    [[nodiscard]] BlendMode blendMode() const noexcept { return m_blend; }
    void setBlendMode(BlendMode m) { m_blend = m; }

    [[nodiscard]] CachePolicy cachePolicy() const noexcept { return m_cache; }
    void setCachePolicy(CachePolicy c) { m_cache = c; }

    // Ordem de avaliacao entre nos de mesma camada de mesclagem. Menor = mais
    // ao fundo. Automatizado pelo grafo, sobrescrito quando o usuario fixa.
    [[nodiscard]] int passIndex() const noexcept { return m_pass; }
    void setPassIndex(int p) { m_pass = p; m_passManual = true; }
    [[nodiscard]] bool passIndexManual() const noexcept { return m_passManual; }
    void setPassIndexManual(bool m) { m_passManual = m; }

    // Substitui o id. So o carregador de projeto e a clonagem usam isto.
    void setId(NodeId id) { m_id = id; }

    // --- Posicao no editor de nos --------------------------------------------
    [[nodiscard]] Vec2 graphPos() const noexcept { return m_graphPos; }
    void setGraphPos(const Vec2& p) { m_graphPos = p; }
    [[nodiscard]] bool isCollapsed() const noexcept { return flag(NodeFlag::Collapsed); }

    // --- Propriedades ---------------------------------------------------------
    [[nodiscard]] const PropertyMap& properties() const noexcept { return m_props; }
    [[nodiscard]] PropertyMap& properties() noexcept { return m_props; }
    [[nodiscard]] const Property* findProperty(const std::string& name) const;
    [[nodiscard]] Property* findProperty(const std::string& name);
    Property& property(const std::string& name);
    bool addProperty(Property p);
    bool removeProperty(const std::string& name);
    [[nodiscard]] std::vector<std::string> propertyNames() const;

    // Atalhos usados com frequencia.
    [[nodiscard]] double opacityAt(Time t) const;
    [[nodiscard]] Transform transformAt(Time t) const;
    void setTransformProperties();   // cria position/scale/rotation/anchor/opacity

    // --- Conexoes -------------------------------------------------------------
    [[nodiscard]] int inputCount() const noexcept { return static_cast<int>(m_inputs.size()); }
    [[nodiscard]] NodeId input(int slot) const;
    void setInput(int slot, NodeId source, int sourceOutput = 0);
    void clearInput(int slot);
    [[nodiscard]] bool isConnected(int slot) const;
    [[nodiscard]] int inputOutputIndex(int slot) const;
    void setInputOutputIndex(int slot, int outIndex);
    // Substitui a fonte em todos os slots que apontam para 'from'.
    void redirectInput(NodeId from, NodeId to);
    void clearAllInputs() { for (auto& s : m_inputs) s = NodeInput{}; }

    struct NodeInput {
        NodeId node = kInvalidId;
        int outputIndex = 0;
    };
    [[nodiscard]] const std::vector<NodeInput>& inputs() const noexcept { return m_inputs; }

    // Parentesco 3D (AE "parenting"). O no herda a transformacao do pai.
    [[nodiscard]] NodeId parent() const noexcept { return m_parent; }
    void setParent(NodeId p) { m_parent = p; }

private:
    NodeId m_id = kInvalidId;
    std::string m_type;
    std::string m_name;
    bool m_enabled = true;
    bool m_passManual = false;
    int m_pass = 0;
    NodeFlag m_flags = NodeFlag::None;
    BlendMode m_blend = BlendMode::Over;
    CachePolicy m_cache = CachePolicy::Never;
    Vec2 m_graphPos{0.0, 0.0};
    NodeId m_parent = kInvalidId;
    PropertyMap m_props;
    std::vector<NodeInput> m_inputs;
};

}  // namespace lmn
