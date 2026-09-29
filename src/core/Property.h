// Property.h - Propriedade animavel.
//
// Uma propriedade carrega o valor base, a curva de keyframes e (opcionalmente)
// uma expressao no estilo After Effects. A precedencia em evaluate() e:
//   1. expressao, se habilitada e com resultado valido
//   2. keyframes, se houver ao menos uma
//   3. valor base
//
// A Property nao conhece o motor de expressoes: ela apenas delega a um backend
// instalado por src/expr. Isso mantem o nucleo sem dependencias.
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Types.h"
#include "Value.h"

namespace lmn {

class Property;

// Como o segmento que sai de uma keyframe chega ate a seguinte.
enum class Interp : uint8_t {
    Linear = 0,  // reta
    Bezier,      // suave, Amount controla a tangente
    Hold,        // degrau (mantem o valor ate a proxima key)
};

const char* interpName(Interp i) noexcept;
Interp interpFromName(const std::string& s) noexcept;

struct Keyframe {
    Time time = 0.0;
    Value value;
    Interp interp = Interp::Bezier;
    // Amount 0..1. easeOut suaviza a saida desta key, easeIn suaviza a entrada
    // da seguinte - e o equivalente ao "Easy Ease"/"Influence" do AE.
    double easeOut = 0.0;
    double easeIn = 0.0;

    // Velocidade relativa (1.0 = uniforme). Usada por slow motion e por
    // expressoes que reamostram curvas.
    double speed = 1.0;

    [[nodiscard]] bool isLinear() const {
        return interp == Interp::Linear || (easeIn == 0.0 && easeOut == 0.0);
    }
};

// Contexto passado ao backend de expressoes.
struct ExpressionContext {
    Time time = 0.0;
    Time inPoint = 0.0;
    Time outPoint = 0.0;
    double frameRate = 30.0;
    Size compositionSize{1920, 1080};
    NodeId nodeId = kInvalidId;
    CompId compId = 0;
    uint32_t index = 0;      // indice do no dentro da composicao
    uint32_t seed = 0;       // semente estavel por no, para wiggle/ruido
    std::string name;        // nome da propriedade
    bool isLayerSafe = true;

    // Le uma propriedade vizinha do mesmo no durante a avaliacao.
    // Caminhos: "position", "scale", "effect:Blur.Amount".
    std::function<bool(const std::string& path, Value& out)> resolveSibling;
};

// Interface implementada por src/expr para resolver expressoes.
class ExpressionBackend {
public:
    virtual ~ExpressionBackend() = default;
    // Retorna false quando a expressao falha; nesse caso o Property usa a curva.
    virtual bool evaluate(const Property& prop, const ExpressionContext& ctx, Value& out) = 0;
};

void setExpressionBackend(std::shared_ptr<ExpressionBackend> backend);
std::shared_ptr<ExpressionBackend> expressionBackend();

// Como o controle aparece na UI (slider, cor,angulo...) e como e serializado.
enum class PropertyUi : uint8_t {
    Default = 0,
    Slider,
    Angle,     // 0..360
    Checkbox,
    Color,
    Position,  // vec2 arrastavel no viewer
    Text,
    FilePath,  // string que abre seletor
    Enum,      // string com lista de opcoes
};

class Property {
public:
    Property() = default;
    explicit Property(Value defaultValue);
    Property(Value defaultValue, PropertyUi ui);

    // --- Identificacao ---------------------------------------------------------
    [[nodiscard]] const std::string& name() const noexcept { return m_name; }
    void setName(std::string n) { m_name = std::move(n); }

    [[nodiscard]] ValueType type() const noexcept { return m_type; }
    [[nodiscard]] PropertyUi ui() const noexcept { return m_ui; }
    void setUi(PropertyUi ui) { m_ui = ui; }

    // Forca o tipo sem alterar o valor. Usado pelo carregador de projeto, que
    // descobre o tipo no arquivo e precisa de uma Property coerente antes de
    // aplicar o valor.
    void setForceType(ValueType t) { m_type = t; }

    // Secao no inspector ("Transform", "Blur"...). Agrupar evita uma lista
    // gigante e sem separacao em nos com muitos parametros.
    [[nodiscard]] const std::string& group() const noexcept { return m_group; }
    void setGroup(std::string g) { m_group = std::move(g); }
    [[nodiscard]] bool groupIsDefault() const noexcept { return m_group.empty(); }

    [[nodiscard]] const std::string& tooltip() const noexcept { return m_tooltip; }
    void setTooltip(std::string t) { m_tooltip = std::move(t); }

    [[nodiscard]] bool isAnimatable() const noexcept { return m_animatable; }
    void setAnimatable(bool a) { m_animatable = a; }

    // Faixa sugerida para sliders. Nao e clamp automatico: valores fora da faixa
    // sao validos (o clamp e opt-in por propriedade, como no AE).
    [[nodiscard]] double minimum() const noexcept { return m_min; }
    [[nodiscard]] double maximum() const noexcept { return m_max; }
    void setRange(double lo, double hi) { m_min = lo; m_max = hi; }
    [[nodiscard]] bool clampsToRange() const noexcept { return m_clamps; }
    void setClampsToRange(bool c) { m_clamps = c; }

    // --- Valor ----------------------------------------------------------------
    [[nodiscard]] const Value& baseValue() const noexcept { return m_base; }
    void setBaseValue(const Value& v);

    // Valor no tempo t, respeitando expressao -> keyframes -> base.
    [[nodiscard]] Value evaluate(Time t) const;
    [[nodiscard]] Value evaluate(Time t, const ExpressionContext& ctx) const;

    // --- Keyframes ------------------------------------------------------------
    [[nodiscard]] const std::vector<Keyframe>& keyframes() const noexcept { return m_keys; }
    std::vector<Keyframe>& keyframes() noexcept { return m_keys; }
    [[nodiscard]] bool hasAnimation() const noexcept { return !m_keys.empty(); }

    // Insere/substitui a key em t. Retorna o indice criado ou UINT32_MAX se a
    // propriedade nao e animavel.
    uint32_t setKeyframe(Time t, const Value& v, bool preserveEase = true);
    [[nodiscard]] uint32_t keyframeIndexAt(Time t, double tolerance = 1e-6) const;
    bool removeKeyframeAt(Time t, double tolerance = 1e-6);
    void clearKeyframes() { m_keys.clear(); m_dirty = true; }

    // Amostra a curva ignorando expressoes - e o que o grafico de keyframes e a
    // timeline desenham.
    [[nodiscard]] Value sampleCurve(Time t) const;
    [[nodiscard]] Value evaluateSegment(Time t, uint32_t index) const;

    void sortKeyframes();
    void markDirty() const noexcept { m_dirty = true; }
    [[nodiscard]] bool isDirty() const noexcept { return m_dirty; }

    // --- Expressao ------------------------------------------------------------
    [[nodiscard]] const std::string& expression() const noexcept { return m_expression; }
    void setExpression(std::string e);
    [[nodiscard]] bool hasExpression() const noexcept { return !m_expression.empty(); }
    [[nodiscard]] bool expressionEnabled() const noexcept { return m_exprEnabled; }
    void setExpressionEnabled(bool e) { m_exprEnabled = e; }

    friend bool operator==(const Property& a, const Property& b);

private:
    void clampToRange(Value& v) const;

    std::string m_name;
    Value m_base;
    ValueType m_type = ValueType::None;
    std::vector<Keyframe> m_keys;
    std::string m_expression;
    std::string m_group;
    std::string m_tooltip;
    PropertyUi m_ui = PropertyUi::Default;
    double m_min = 0.0;
    double m_max = 1.0;
    bool m_animatable = true;
    bool m_clamps = false;
    bool m_exprEnabled = true;
    mutable bool m_dirty = true;
};

using PropertyMap = std::map<std::string, Property>;

}  // namespace lmn
