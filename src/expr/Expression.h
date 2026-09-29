// Expression.h - Motor de expressoes no estilo After Effects.
//
// Linguagem deliberadamente pequena: numeros, texto, vetores, operadores,
// chamadas de funcao e acesso a propriedades. Sem alocacoes por frame no
// caminho quente e com o texto compilado uma unica vez (por string).
//
//   wiggle(2, 0.5)
//   loopOut("cycle", 0.0)
//   transform.position[0] + 100
//   Math.sin(time * 6.28) * amplitude
//
// Semanticas de loop* seguem o AE: keyframes antes do loop sao estendidas
// ("continue") ou repetidas ("cycle") ou espelhadas ("pingpong").
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/Property.h"
#include "core/Value.h"

namespace lmn::expr {

// Como um loop trata a parte da curva anterior ao inicio do loop.
enum class LoopType : uint8_t {
    None = 0,
    Cycle,
    PingPong,
    Continue,
    Offset,
    Count,
};

LoopType loopTypeFromName(const std::string& s) noexcept;

// Modo de execucao de um no: em que dominio o loop* opera.
struct LoopSpec {
    LoopType type = LoopType::Cycle;
    double cyclesBefore = 0.0;   // numero de repeticoes do trecho inicial
    double cycleKeyframes = 0.0;// tamanho do ciclo em keyframes (0 = do inicio)
    double pingpong = 0.0;
    double continueOut = 0.0;
    double offset = 0.0;
    [[nodiscard]] bool isValid() const {
        return type != LoopType::None && type != LoopType::Count;
    }
};

// Resultado da compilacao de um texto de expressao.
struct ParseResult {
    bool ok = false;
    std::string error;       // mensagem legivel quando !ok
    int errorLine = 0;
    int errorColumn = 0;
    std::shared_ptr<void> ast;  // ExprNode* ; opaco aqui para nao vazar o AST
};

// Contexto de avaliacao. Mesmos campos do ExpressionContext do core, mais o
// que a expressao precisa olhar para fora.
struct EvalContext {
    const ExpressionContext* ctx = nullptr;
    // Le a curva de uma propriedade vizinha (ex.: transform.position).
    // Retorna false se a propriedade nao existir.
    std::function<bool(const std::string& path, Value& out)> resolveSibling;
    // Valor da propria propriedade no tempo (a keyword 'value').
    const Property* self = nullptr;
};

class Evaluator;

class Engine {
public:
    static Engine& instance();

    // Compila (com cache por texto) a expressao de uma propriedade.
    // Retorna nullptr em caso de erro e preenche 'error'.
    const void* compile(const std::string& source, std::string* error,
                        int* line = nullptr, int* column = nullptr);

    // Avalia uma expressao ja compilada.
    bool evaluate(const void* compiled, const EvalContext& ctx, Value& out) const;

    // Avalia texto avulso, compilando se necessario.
    bool evaluateSource(const std::string& source, const EvalContext& ctx, Value& out,
                        std::string* error = nullptr);

    // Mensagem do ultimo erro de avaliacao, para o painel de diagnostico.
    [[nodiscard]] std::string lastError() const;

    // Implementa ExpressionBackend do core, resolvendo Properties com
    // expressao via este motor.
    [[nodiscard]] std::shared_ptr<ExpressionBackend> backend();

    // Limpa o cache de expressoes compiladas (troca de arquivo de projeto).
    void clearCache();
    [[nodiscard]] size_t cacheSize() const;

    // Registra funcoes globais adicionais. Usado por nos para expor helpers
    // (ex.: blur.matteMode("add")).
    using NativeFn = std::function<Value(const std::vector<Value>&)>;
    void setFunction(const std::string& name, NativeFn fn);

private:
    Engine() = default;
    struct Impl;
    std::shared_ptr<Impl> m_impl;
};

// Estruturas de AST expostas para quem quiser estender (testes, tooling).
struct ExprNode;
using ExprPtr = std::unique_ptr<ExprNode>;

// Constrói um backend e registra em core::setExpressionBackend.
void installAsCoreBackend();

}  // namespace lmn::expr
