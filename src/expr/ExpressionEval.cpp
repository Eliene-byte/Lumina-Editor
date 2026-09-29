#include <algorithm>
#include <cmath>
#include <functional>
#include <hash>
#include <map>

#include "ExpressionInternal.h"

namespace lmn::expr {
namespace {

// Resultado de avaliar um no: um valor concreto ou um namespace de propriedades
// (transform, effect, thisComp...).
struct Res {
    enum class Kind : uint8_t { Value, Namespace };
    Kind kind = Kind::Value;
    Value value;
    std::map<std::string, Value> ns;

    static Res of(Value v) { Res r; r.value = std::move(v); return r; }
    static Res ns() { Res r; r.kind = Kind::Namespace; return r; }
};

constexpr int kMaxDepth = 64;
constexpr double kPi = 3.14159265358979323846;

double safeDiv(double a, double b) { return std::abs(b) < 1e-12 ? 0.0 : a / b; }

bool truthy(const Value& v) {
    switch (v.type()) {
        case ValueType::Bool:   return v.asBool();
        case ValueType::String: return !v.asString().empty();
        default:                return std::abs(v.toScalar()) > 1e-12;
    }
}

Color hslToRgb(double h, double s, double v, double a) {
    h = std::fmod(std::fmod(h, 360.0) + 360.0, 360.0) / 360.0;
    s = std::clamp(s, 0.0, 1.0);
    v = std::clamp(v, 0.0, 1.0);
    const auto i = static_cast<int>(std::floor(h * 6.0));
    const double f = h * 6.0 - i;
    const double p = v * (1.0 - s);
    const double q = v * (1.0 - f * s);
    const double t = v * (1.0 - (1.0 - f) * s);
    switch (i % 6) {
        case 0:  return Color{v, t, p, a};
        case 1:  return Color{q, v, p, a};
        case 2:  return Color{p, v, t, a};
        case 3:  return Color{p, q, v, a};
        case 4:  return Color{t, p, v, a};
        default: return Color{v, p, q, a};
    }
}

class Interpreter {
public:
    Interpreter(const EvalContext& ctx, const std::map<std::string, Engine::NativeFn>& fns)
        : m_ctx(ctx), m_fns(fns) {}

    Res run(const ExprNode* root) { return eval(root, 0); }
    [[nodiscard]] const std::string& error() const { return m_error; }

private:
    Res eval(const ExprNode* n, int depth) {
        if (!n) { m_error = "no invalido"; return Res::of(Value()); }
        if (depth > kMaxDepth) { m_error = "expressao profunda demais"; return Res::of(Value()); }
        switch (n->kind) {
            case NodeKind::Number:  return Res::of(Value(n->num));
            case NodeKind::String:  return Res::of(Value(n->text));
            case NodeKind::Ident:   return evalIdent(n);
            case NodeKind::Member:  return evalMember(n, depth);
            case NodeKind::Index:   return evalIndex(n, depth);
            case NodeKind::Unary:   return evalUnary(n, depth);
            case NodeKind::Binary:  return evalBinary(n, depth);
            case NodeKind::Ternary: return evalTernary(n, depth);
            case NodeKind::Call:    return evalCall(n, depth);
        }
        m_error = "tipo de no desconhecido";
        return Res::of(Value());
    }

    Res evalIdent(const ExprNode* n) {
        const std::string& id = n->text;
        const ExpressionContext* c = m_ctx.ctx;
        const double t = c ? c->time : 0.0;

        if (id == "value")    return Res::of(selfValue(t));
        if (id == "time")     return Res::of(Value(t));
        if (id == "index")    return Res::of(Value(static_cast<double>(c ? c->index : 0)));
        if (id == "seed")     return Res::of(Value(static_cast<double>(c ? c->seed : 0)));
        if (id == "width")    return Res::of(Value(c ? static_cast<double>(c->compositionSize.width) : 1920.0));
        if (id == "height")   return Res::of(Value(c ? static_cast<double>(c->compositionSize.height) : 1080.0));
        if (id == "fps")      return Res::of(Value(c ? c->frameRate : 30.0));
        if (id == "inPoint")  return Res::of(Value(c ? c->inPoint : 0.0));
        if (id == "outPoint") return Res::of(Value(c ? c->outPoint : 0.0));

        if (id == "thisComp" || id == "thisLayer") {
            Res r = Res::ns();
            r.ns["time"] = Value(t);
            r.ns["duration"] = Value(c ? std::max(0.0, c->outPoint - c->inPoint) : 0.0);
            r.ns["width"] = Value(c ? static_cast<double>(c->compositionSize.width) : 1920.0);
            r.ns["height"] = Value(c ? static_cast<double>(c->compositionSize.height) : 1080.0);
            r.ns["fps"] = Value(c ? c->frameRate : 30.0);
            r.ns["inPoint"] = Value(c ? c->inPoint : 0.0);
            r.ns["outPoint"] = Value(c ? c->outPoint : 0.0);
            r.ns["pixelAspect"] = Value(1.0);
            r.ns["index"] = Value(static_cast<double>(c ? c->index : 0));
            r.ns["name"] = Value(c ? c->name : std::string());
            return r;
        }

        if (id == "transform" || id == "effect" || id == "source" || id == "comp" ||
            id == "thisNode") {
            return Res::ns();
        }

        m_error = "identificador desconhecido: " + id;
        return Res::of(Value());
    }

    Res evalMember(const ExprNode* n, int depth) {
        const Res base = eval(n->children[0].get(), depth + 1);
        if (!m_error.empty()) return base;
        const std::string& name = n->text;

        if (base.kind == Res::Kind::Namespace) {
            const auto it = base.ns.find(name);
            if (it != base.ns.end()) return Res::of(it->second);

            // Namespace vazio de "effect(...)": o caminho completo vem do no.
            const auto key = base.ns.find("__path");
            const std::string path =
                (key != base.ns.end() && name != "__path")
                    ? key->second.asString() + "." + name
                    : name;

            if (m_ctx.resolveSibling && m_ctx.resolveSibling(path, base.value)) {
                return Res::of(base.value);
            }
            // Padroes de transformacao, para expresseses que rodam fora de um
            // contexto com resolver completo.
            if (name == "position") return Res::of(Value(Vec2{}));
            if (name == "scale")    return Res::of(Value(Vec2{100.0, 100.0}));
            if (name == "rotation") return Res::of(Value(0.0));
            if (name == "anchor")   return Res::of(Value(Vec2{}));
            if (name == "opacity")  return Res::of(Value(1.0));
            m_error = "propriedade desconhecida: " + name;
            return Res::of(Value());
        }

        const Vec4 v = base.value.toVec4();
        if (name == "x" || name == "0") return Res::of(Value(v.x));
        if (name == "y" || name == "1") return Res::of(Value(v.y));
        if (name == "z" || name == "2") return Res::of(Value(v.z));
        if (name == "w" || name == "3" || name == "a" || name == "alpha") {
            return Res::of(Value(v.w));
        }
        if (name == "length") return Res::of(Value(v.length()));
        m_error = "propriedade desconhecida: " + name;
        return Res::of(Value());
    }

    Res evalIndex(const ExprNode* n, int depth) {
        const Res base = eval(n->children[0].get(), depth + 1);
        const Res idx = eval(n->children[1].get(), depth + 1);
        if (!m_error.empty()) return base;

        const int i = static_cast<int>(idx.value.toScalar());
        const Vec4 v = base.value.toVec4();
        switch (i) {
            case 0: return Res::of(Value(v.x));
            case 1: return Res::of(Value(v.y));
            case 2: return Res::of(Value(v.z));
            case 3: return Res::of(Value(v.w));
            default:
                m_error = "indice fora de 0..3: " + std::to_string(i);
                return Res::of(Value());
        }
    }

    Res evalUnary(const ExprNode* n, int depth) {
        const Res v = eval(n->children[0].get(), depth + 1);
        if (!m_error.empty()) return v;
        if (n->text == "-") return Res::of(v.value.scale(-1.0));
        return Res::of(Value(!truthy(v.value)));
    }

    Res evalTernary(const ExprNode* n, int depth) {
        const Res cond = eval(n->children[0].get(), depth + 1);
        if (!m_error.empty()) return cond;
        return truthy(cond.value) ? eval(n->children[1].get(), depth + 1)
                                 : eval(n->children[2].get(), depth + 1);
    }

    Res evalBinary(const ExprNode* n, int depth) {
        const std::string& op = n->text;

        // Atalhos logicos: o lado direito so e avaliado quando importa.
        if (op == "&" || op == "|") {
            const Res l = eval(n->children[0].get(), depth + 1);
            if (!m_error.empty()) return l;
            const bool lv = truthy(l.value);
            if (op == "&" && !lv) return Res::of(Value(false));
            if (op == "|" && lv) return Res::of(Value(true));
            const Res r = eval(n->children[1].get(), depth + 1);
            if (!m_error.empty()) return r;
            return Res::of(Value(truthy(r.value)));
        }

        const Res l = eval(n->children[0].get(), depth + 1);
        if (!m_error.empty()) return l;
        const Res r = eval(n->children[1].get(), depth + 1);
        if (!m_error.empty()) return r;

        if (op == "=") return Res::of(Value(l.value == r.value));
        if (op == "!") return Res::of(Value(!(l.value == r.value)));

        // Vetores operam componente a componente, evitando ramificar por tipo
        // em cada operador.
        const ValueType lt = l.value.type();
        if (lt == ValueType::Vec2 || lt == ValueType::Vec3 || lt == ValueType::Vec4) {
            const Vec4 a = l.value.toVec4();
            const Vec4 b = r.value.toVec4();
            if (op == "<")  return Res::of(Value(a.x < b.x));
            if (op == ">")  return Res::of(Value(a.x > b.x));
            if (op == "<=") return Res::of(Value(a.x <= b.x));
            if (op == ">=") return Res::of(Value(a.x >= b.x));
            if (op == "+")  return Res::of(Value(Vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w)));
            if (op == "-")  return Res::of(Value(Vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w)));
            if (op == "*")  return Res::of(Value(a * b));
            if (op == "/") {
                return Res::of(Value(Vec4(safeDiv(a.x, b.x), safeDiv(a.y, b.y),
                                         safeDiv(a.z, b.z), safeDiv(a.w, b.w))));
            }
            m_error = "operador '" + op + "' nao se aplica a vetores";
            return Res::of(Value());
        }

        const double a = l.value.toScalar();
        const double b = r.value.toScalar();
        if (op == "+")  return Res::of(Value(a + b));
        if (op == "-")  return Res::of(Value(a - b));
        if (op == "*")  return Res::of(Value(a * b));
        if (op == "/")  return Res::of(Value(safeDiv(a, b)));
        if (op == "%")  return Res::of(Value(b != 0.0 ? std::fmod(a, b) : 0.0));
        if (op == "<")  return Res::of(Value(a < b));
        if (op == ">")  return Res::of(Value(a > b));
        if (op == "<=") return Res::of(Value(a <= b));
        if (op == ">=") return Res::of(Value(a >= b));

        m_error = "operador desconhecido: " + op;
        return Res::of(Value());
    }

    Res evalCall(const ExprNode* n, int depth) {
        std::string name = n->text;

        std::vector<Value> args;
        args.reserve(n->children.size());
        for (const auto& c : n->children) {
            const Res r = eval(c.get(), depth + 1);
            if (!m_error.empty()) return r;
            args.push_back(r.value);
        }

        // effect("Blur") devolve um namespace; o membro seguinte vira o caminho
        // completo e e resolvido por resolveSibling.
        if (name == "effect") {
            Res r = Res::ns();
            r.ns["__path"] = Value("effect:" + (args.empty() ? std::string() : args[0].asString()));
            return r;
        }
        if (name.rfind("effect.", 0) == 0) {
            Value out;
            if (m_ctx.resolveSibling && m_ctx.resolveSibling(name, out)) return Res::of(out);
            m_error = "parametro de efeito desconhecido: " + name.substr(7);
            return Res::of(Value());
        }

        if (name.rfind("Math.", 0) == 0) name = name.substr(5);

        const auto native = m_fns.find(name);
        if (native != m_fns.end()) return Res::of(native->second(args));

        return Res::of(callBuiltin(name, args));
    }

    Value selfValue(Time t) const {
        return m_ctx.self ? m_ctx.self->sampleCurve(t) : Value(0.0);
    }

    Value callBuiltin(const std::string& name, const std::vector<Value>& args) {
        const auto arg = [&](size_t i) -> double {
            return i < args.size() ? args[i].toScalar() : 0.0;
        };
        const auto argv = [&](size_t i) -> Vec4 {
            return i < args.size() ? args[i].toVec4() : Vec4{};
        };
        const ExpressionContext* c = m_ctx.ctx;
        const double t = c ? c->time : 0.0;

        // --- matematica ---
        if (name == "sin")   return Value(std::sin(arg(0)));
        if (name == "cos")   return Value(std::cos(arg(0)));
        if (name == "tan")   return Value(std::tan(arg(0)));
        if (name == "abs")   return Value(std::abs(arg(0)));
        if (name == "sqrt")  return Value(std::sqrt(std::max(0.0, arg(0))));
        if (name == "exp")   return Value(std::exp(arg(0)));
        if (name == "log")   return Value(arg(0) > 0.0 ? std::log(arg(0)) : 0.0);
        if (name == "floor") return Value(std::floor(arg(0)));
        if (name == "ceil")  return Value(std::ceil(arg(0)));
        if (name == "round") return Value(std::round(arg(0)));
        if (name == "sign")  return Value(arg(0) > 0.0 ? 1.0 : (arg(0) < 0.0 ? -1.0 : 0.0));
        if (name == "min")   return Value(args.size() < 2 ? arg(0) : std::min(arg(0), arg(1)));
        if (name == "max")   return Value(args.size() < 2 ? arg(0) : std::max(arg(0), arg(1)));
        if (name == "pow")   return Value(std::pow(arg(0), arg(1)));
        if (name == "clamp") {
            return Value(std::clamp(arg(0), std::min(arg(1), arg(2)), std::max(arg(1), arg(2))));
        }
        if (name == "lerp" || name == "mix") {
            const double a0 = arg(0), a1 = arg(1);
            return Value(a0 + (a1 - a0) * arg(2));
        }
        if (name == "smoothstep") {
            const double e0 = arg(0), e1 = arg(1), x = arg(2);
            return Value(smoothStepEasing(e1 == e0 ? 0.0 : std::clamp((x - e0) / (e1 - e0),
                                                                    0.0, 1.0)));
        }
        if (name == "deg" || name == "toDeg" || name == "radiansToDegrees") {
            return Value(arg(0) * 180.0 / kPi);
        }
        if (name == "rad" || name == "toRad" || name == "degreesToRadians") {
            return Value(arg(0) * kPi / 180.0);
        }
        if (name == "posterize") {
            const double levels = std::max(1.0, arg(1));
            return Value(std::round(arg(0) * levels) / levels);
        }
        if (name == "random") {
            const uint64_t s = static_cast<uint64_t>(std::hash<std::string>{}(name)) ^
                               (args.size() > 2 ? static_cast<uint64_t>(arg(2)) * 2654435761u : 0u);
            return Value(hashToUnit(s, static_cast<int64_t>(arg(0) * 1000.0)) *
                             (arg(1) - arg(0)) + arg(0));
        }
        if (name == "seedRandom") return Value(0.0);

        // --- vetores e cores ---
        if (name == "length") return Value(argv(0).length());
        if (name == "dot")    return Value(argv(0).x * argv(1).x + argv(0).y * argv(1).y);
        if (name == "normalize") {
            const Vec4 v = argv(0);
            const double len = std::hypot(v.x, v.y);
            if (len < 1e-9) return Value(Vec4{});
            return Value(Vec4(v.x / len, v.y / len, v.z / len, v.w));
        }
        if (name == "rgb" || name == "rgba") {
            return Value(Color(arg(0), arg(1), arg(2), args.size() > 3 ? arg(3) : 1.0));
        }
        if (name == "hsl" || name == "hsla") {
            return Value(hslToRgb(arg(0), arg(1), arg(2), args.size() > 3 ? arg(3) : 1.0));
        }
        if (name == "clampRGB") {
            const Vec4 v = argv(0);
            return Value(Color(std::clamp(v.x, 0.0, 1.0), std::clamp(v.y, 0.0, 1.0),
                               std::clamp(v.z, 0.0, 1.0), v.w));
        }

        // --- tempo e curvas ---
        if (name == "linear") {
            const double tt = arg(0), t1 = arg(1), t2 = arg(2), v1 = arg(3), v2 = arg(4);
            if (t2 == t1) return Value(v2);
            return Value(v1 + (v2 - v1) * std::clamp((tt - t1) / (t2 - t1), 0.0, 1.0));
        }
        if (name == "ease" || name == "easeIn" || name == "easeOut") {
            const double tt = std::clamp(arg(0), 0.0, 1.0);
            const double amt = args.size() > 1 ? arg(1) : 0.333;
            if (name == "ease")    return Value(cubicBezierEasing(tt, amt, amt));
            if (name == "easeIn")  return Value(cubicBezierEasing(tt, 1.0 - amt, 0.0));
            return Value(cubicBezierEasing(tt, 0.0, 1.0 - amt));
        }
        if (name == "posterizeTime") {
            const double fps = arg(0) > 0.0 ? arg(0) : 1.0;
            return Value(std::floor(t * fps) / fps);
        }
        if (name == "valueAtTime") return selfValue(arg(0));
        if (name == "hold" || name == "holds") return selfValue(t);

        if (name == "loopOut" || name == "loopIn") return loop(name, args);
        if (name == "loopDuration" || name == "loopTime") {
            return Value(c ? std::max(0.0, c->outPoint - c->inPoint) : 0.0);
        }

        if (name == "wiggle" || name == "randomize") return wiggle(args);

        m_error = "funcao desconhecida: " + name;
        return Value();
    }

    // Sem estado entre frames: o valor depende so de (tempo, seed, argumentos).
    Value wiggle(const std::vector<Value>& args) {
        const double freq = args.size() > 0 && args[0].toScalar() > 0.0 ? args[0].toScalar() : 1.0;
        const double amp = args.size() > 1 ? args[1].toScalar() : 1.0;
        const double phase = args.size() > 2 ? args[2].toScalar() : 0.0;
        const ExpressionContext* c = m_ctx.ctx;
        const double t = (c ? c->time : 0.0) + phase;
        const uint64_t seed = static_cast<uint64_t>(c ? c->seed : 0) * 2654435761u + 1u;

        const Value base = selfValue(t);
        if (args.size() >= 2 && args[1].type() == ValueType::Vec2) {
            const Vec2 b = base.asVec2();
            return Value(Vec2{b.x + fractalNoise(seed, t * freq, 3) * amp,
                              b.y + fractalNoise(seed ^ 0x9E3779B9u, t * freq, 3) * amp});
        }
        return Value(base.toScalar() + fractalNoise(seed, t * freq, 3) * amp);
    }

    // Loop no estilo After Effects. A janela e [inPoint, outPoint] da
    // composicao; o trecho e reamostrado e depois convertido de volta.
    Value loop(const std::string& name, const std::vector<Value>& args) {
        const ExpressionContext* c = m_ctx.ctx;
        if (!c || !m_ctx.self) {
            m_error = name + "() exige uma propriedade animada";
            return Value();
        }

        LoopSpec spec;
        spec.type = args.empty() ? LoopType::Cycle
                                 : loopTypeFromName(args[0].type() == ValueType::String
                                                        ? args[0].asString()
                                                        : "cycle");
        if (spec.type == LoopType::None) spec.type = LoopType::Cycle;
        if (args.size() > 1) spec.cyclesBefore = args[1].toScalar();

        const double wStart = c->inPoint;
        const double wEnd = c->outPoint;
        const double wLen = wEnd - wStart;
        if (wLen <= kEpsilon) {
            m_error = name + "() sem intervalo valido";
            return Value();
        }

        // loopIn conta a partir do fim; loopOut a partir do inicio.
        const double phase = (name == "loopIn") ? (wEnd - c->time) : (c->time - wStart);
        if (phase < 0.0) return selfValue(c->time);

        double sampleAt = wStart;
        switch (spec.type) {
            case LoopType::PingPong: {
                const double u = std::fmod(phase, 2.0 * wLen);
                sampleAt = wStart + (u <= wLen ? u : 2.0 * wLen - u);
                break;
            }
            case LoopType::Continue:
                sampleAt = wEnd + extrapolate(wEnd, wLen, phase);
                break;
            case LoopType::Offset: {
                sampleAt = wStart + std::fmod(phase, wLen);
                const Value shifted = selfValue(sampleAt).add(Value(phase - std::fmod(phase, wLen)));
                return shifted;
            }
            case LoopType::Cycle:
            default: {
                const double cycles = spec.cyclesBefore > 0.0 ? spec.cyclesBefore : 1.0;
                const double period = wLen / std::max(1.0, cycles);
                sampleAt = wStart + std::fmod(phase, period);
                break;
            }
        }
        return selfValue(sampleAt);
    }

    // Estende a curva alem do fim pela tangente do ultimo trecho conhecido.
    double extrapolate(double wEnd, double wLen, double phase) {
        const double dt = std::max(1e-3, wLen * 0.1);
        const double v1 = selfValue(wEnd).toScalar();
        const double v0 = selfValue(wEnd - dt).toScalar();
        return phase * (v1 - v0) / dt;
    }

    const EvalContext& m_ctx;
    const std::map<std::string, Engine::NativeFn>& m_fns;
    std::string m_error;
};

}  // namespace

bool runInterpreter(const ExprNode* root, const EvalContext& ctx,
                    const std::map<std::string, Engine::NativeFn>& fns, Value& out,
                    std::string* error) {
    Interpreter interp(ctx, fns);
    const Res r = interp.run(root);
    if (!interp.error().empty()) {
        if (error) *error = interp.error();
        return false;
    }
    if (r.kind != Res::Kind::Value) {
        if (error) *error = "a expressao precisa terminar em um valor";
        return false;
    }
    out = r.value;
    return true;
}

}  // namespace lmn::expr
