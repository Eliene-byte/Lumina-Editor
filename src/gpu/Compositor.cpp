#include "Compositor.h"

#include <algorithm>
#include <cstdlib>

#include <QElapsedTimer>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>

namespace lmn::gpu {

// Um VAO vazio e um unico triangulo de tela cheia cobrem todos os passes: nao
// existe geometria para upload em nenhum ponto do render.
namespace {

class ScreenQuad {
public:
    static ScreenQuad& instance() {
        static ScreenQuad q;
        return q;
    }

    void ensure() {
        if (m_vao) return;
        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);
        glBindVertexArray(0);
    }

    void draw() {
        ensure();
        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
    }

    void invalidate() { m_vao = 0; }

private:
    GLuint m_vao = 0;
};

}  // namespace

void fullscreenTriangle() {
    ScreenQuad::instance().draw();
}

// ---------------------------------------------------------------------------
// RenderContext
// ---------------------------------------------------------------------------

RenderTargetPtr RenderContext::acquire() {
    auto target = m_pool.acquire(m_width, m_height);
    if (target) m_acquired.push_back(target);
    return target;
}

void RenderContext::releaseAll() {
    for (auto& t : m_acquired) m_pool.release(t);
    m_acquired.clear();
}

// ---------------------------------------------------------------------------
// Compositor
// ---------------------------------------------------------------------------

Compositor::Compositor() = default;
Compositor::~Compositor() = default;

bool Compositor::shaderFor(const Node& node, ShaderProgram*& out) {
    const NodeTypeDesc* desc = NodeRegistry::instance().desc(node.type());
    if (!desc) {
        m_lastError = "tipo de no desconhecido: " + node.type();
        return false;
    }

    const std::string resource = desc->shader.empty() ? std::string(":/shaders/copy.frag")
                                                      : ":" + desc->shader;
    const std::string key = node.type();

    const auto it = m_shaders.find(key);
    if (it != m_shaders.end() && it->second && it->second->valid()) {
        out = it->second.get();
        return true;
    }

    const QString source = ShaderLoader::loadFragment(QString::fromStdString(resource));
    if (source.isEmpty()) {
        m_lastError = "shader nao encontrado: " + resource;
        return false;
    }

    auto program = std::make_unique<ShaderProgram>();
    if (!program->compileFragment(key, source)) {
        m_lastError = "falha ao compilar " + key + ": " + ShaderLog::get(key);
        return false;
    }
    out = program.get();
    m_shaders[key] = std::move(program);
    return true;
}

void Compositor::setInputTexture(ShaderProgram& program, const char* uniform,
                                 GLuint texture, int unit) {
    if (texture) program.bindTexture(uniform, unit, texture);
}

GLuint Compositor::resolveInput(Composition& comp, const Node& node, int slot, Time time,
                                RenderContext& ctx, const Project* project, int depth) {
    if (slot >= node.inputCount()) return 0;
    const NodeId sourceId = node.input(slot);
    if (sourceId == kInvalidId) return 0;

    const auto cached = m_frameResults.find(sourceId);
    if (cached != m_frameResults.end()) return cached->second;

    const Node* source = comp.graph().node(sourceId);
    if (!source) return 0;

    GLuint texture = 0;
    if (!prepareNode(comp, *source, time, ctx, project, texture, depth + 1)) {
        return 0;
    }
    return texture;
}

void Compositor::bindStandardUniforms(ShaderProgram& program, const Node& node, Time time,
                                      const Composition& comp, int width, int height) {
    program.setVec2("u_resolution", Vec2{static_cast<double>(width),
                                         static_cast<double>(height)});
    program.setFloat("u_time", static_cast<float>(time));
    program.setFloat("u_frameRate", static_cast<float>(comp.fps()));
    program.setFloat("u_compWidth", static_cast<float>(comp.size().width));
    program.setFloat("u_compHeight", static_cast<float>(comp.size().height));

    // Transform: a maioria dos nos e um filtro com transformacao propria, mas
    // alguns (merge, solid) nao devem transformar.
    const NodeTypeDesc* desc = NodeRegistry::instance().desc(node.type());
    if (desc && desc->hasTransform) {
        const Transform t = node.transformAt(time);
        program.setMat3T("u_transform", t);
        Transform inv;
        if (t.invert(inv)) program.setMat3T("u_inverseTransform", inv);
        program.setInt("u_useTransform", 1);
    } else {
        program.setInt("u_useTransform", 0);
        program.setMat3T("u_inverseTransform", Transform::identity());
    }

    // Propriedades com nome padrao em todos os nos.
    program.setFloat("u_opacity", static_cast<float>(node.opacityAt(time)));
    if (const Property* blend = node.findProperty("blend")) {
        program.setInt("u_mode", static_cast<int>(blendModeFromName(
                                     blend->evaluate(time).asString("Over").c_str())));
    }
}

bool Compositor::prepareNode(Composition& comp, const Node& node, Time time,
                             RenderContext& ctx, const Project* project,
                             GLuint& outputTexture, int depth) {
    if (depth > 64) {
        m_lastError = "grafo profundo demais (possivel ciclo)";
        return false;
    }

    // Nos desligados passam a entrada direto, sem consumir um alvo.
    if (!node.isEnabled()) {
        outputTexture = resolveInput(comp, node, 0, time, ctx, project, depth);
        m_frameResults[node.id()] = outputTexture;
        return true;
    }

    ShaderProgram* program = nullptr;
    if (!shaderFor(node, program)) return false;

    // Alvo de saida deste no.
    RenderTargetPtr target = ctx.acquire();
    if (!target) {
        m_lastError = "sem memoria de video para renderizar";
        return false;
    }

    // Monta o contexto de expressoes, incluindo a resolucao de propriedades
    // vizinhas (transform.position, effect("Blur").Amount).
    ExpressionContext expr;
    expr.time = time;
    expr.inPoint = comp.duration().in;
    expr.outPoint = comp.duration().out;
    expr.frameRate = comp.fps();
    expr.compositionSize = comp.size();
    expr.nodeId = node.id();
    expr.compId = comp.id();
    expr.seed = static_cast<uint32_t>(node.id() * 2654435761u);

    uint32_t index = 0;
    for (NodeId id : comp.graph().nodeIds()) {
        if (id == node.id()) break;
        ++index;
    }
    expr.index = index;

    const Node* self = &node;
    expr.resolveSibling = [self, time, &expr](const std::string& path, Value& out) {
        if (const Property* p = self->findProperty(path)) {
            out = p->evaluate(time, expr);
            return true;
        }
        // "effect:Nome.Propriedade": procura em qualquer no com esse nome.
        return false;
    };

    target->bindAsTarget();
    program->bind();

    m_nextUnit = 0;
    bindStandardUniforms(*program, node, time, comp, ctx.width(), ctx.height());

    // O uniform de entrada segue um nome previsivel: u_input, u_input1,
    // u_input2... e o flag correspondente u_hasInput, u_hasInput1...
    int unit = 0;
    for (int slot = 0; slot < node.inputCount(); ++slot) {
        const std::string suffix = slot == 0 ? "" : std::to_string(slot);
        const GLuint tex = resolveInput(comp, node, slot, time, ctx, project, depth);
        setInputTexture(*program, ("u_input" + suffix).c_str(), tex, unit++);
        program->setInt(("u_hasInput" + suffix).c_str(), tex ? 1 : 0);
    }

    // O tipo do no preenche os uniforms especificos dele.
    if (const UniformBinder binder = NodeUniformRegistry::instance().find(node.type())) {
        binder(*program, node, time, expr);
    } else {
        // Sem binder registrado, publica as propriedades cujo nome bate com um
        // uniform. Cobre nos simples sem codigo dedicado.
        for (const auto& [name, prop] : node.properties()) {
            switch (prop.type()) {
                case ValueType::Double: program->setFloat(name.c_str(),
                    static_cast<float>(prop.evaluate(time, expr).asDouble(0.0))); break;
                case ValueType::Bool:   program->setBool(name.c_str(),
                    prop.evaluate(time, expr).asBool(false)); break;
                case ValueType::Color:  program->setColor(name.c_str(),
                    prop.evaluate(time, expr).asColor()); break;
                case ValueType::Vec2:   program->setVec2(name.c_str(),
                    prop.evaluate(time, expr).asVec2()); break;
                case ValueType::Vec3:   program->setVec3(name.c_str(),
                    prop.evaluate(time, expr).asVec3()); break;
                case ValueType::Vec4:   program->setVec4(name.c_str(),
                    prop.evaluate(time, expr).toVec4()); break;
                case ValueType::String: {
                    const std::string s = prop.evaluate(time, expr).asString();
                    program->setFloat(name.c_str(),
                                      static_cast<float>(std::atoi(s.c_str())));
                    break;
                }
                default: break;
            }
        }
    }

    ScreenQuad::instance().draw();

    outputTexture = target->texture();
    m_frameResults[node.id()] = outputTexture;
    return true;
}

GLuint Compositor::render(Composition& comp, Time time, RenderContext& ctx,
                          const Project* project, RenderStats* stats) {
    QElapsedTimer timer;
    timer.start();

    m_frameResults.clear();
    m_lastError.clear();
    m_nextUnit = 0;

    // Grafo invalido: nada a renderizar.
    if (comp.graph().output() == kInvalidId) {
        RenderTargetPtr empty = ctx.acquire();
        if (empty) {
            empty->clear(comp.background());
            return empty->texture();
        }
        return 0;
    }

    GLuint result = 0;
    const Node* outputNode = comp.graph().node(comp.graph().output());
    if (!outputNode) return 0;

    if (!prepareNode(comp, *outputNode, time, ctx, project, result, 0)) {
        // Falha em um no: mostra o fundo da composicao em vez de tela preta.
        if (!m_lastError.empty() && !comp.graph().isAcyclic()) {
            m_lastError = "a composicao tem um ciclo";
        }
        RenderTargetPtr fallback = ctx.acquire();
        if (fallback) {
            fallback->clear(comp.background());
            return fallback->texture();
        }
        return 0;
    }

    if (stats) {
        stats->nodesEvaluated = static_cast<int>(m_frameResults.size());
        stats->drawCalls = static_cast<int>(m_frameResults.size());
        stats->texturesAllocated = static_cast<int>(m_frameTargets.size());
        stats->milliseconds = timer.nsecsElapsed() / 1e6;
    }
    return result;
}

}  // namespace lmn::gpu
