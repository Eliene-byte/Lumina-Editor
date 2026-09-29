#include <mutex>
#include <unordered_map>

#include "ExpressionInternal.h"

namespace lmn::expr {

struct Engine::Impl {
    struct Entry {
        std::shared_ptr<ExprNode> root;
    };

    std::mutex mutex;
    std::unordered_map<std::string, Entry> cache;
    std::map<std::string, NativeFn> functions;
    std::string lastError;
};

Engine& Engine::instance() {
    static Engine engine;
    if (!engine.m_impl) engine.m_impl = std::make_shared<Impl>();
    return engine;
}

const void* Engine::compile(const std::string& source, std::string* error, int* line,
                            int* column) {
    auto& impl = *m_impl;
    std::lock_guard lock(impl.mutex);

    const auto cached = impl.cache.find(source);
    if (cached != impl.cache.end()) return cached->second.root.get();

    std::vector<Token> tokens;
    std::string lexError;
    int errLine = 0;
    int errCol = 0;
    Lexer lexer(source);
    if (!lexer.tokenize(tokens, lexError, errLine, errCol)) {
        if (error) *error = lexError;
        if (line) *line = errLine;
        if (column) *column = errCol;
        return nullptr;
    }

    Parser parser(std::move(tokens));
    ExprPtr root = parser.parse();
    if (!root) {
        if (error) *error = parser.errorString();
        if (line) *line = parser.errorLineValue();
        if (column) *column = parser.errorColumnValue();
        return nullptr;
    }

    Impl::Entry entry;
    entry.root = std::move(root);
    const ExprNode* raw = entry.root.get();
    impl.cache.emplace(source, std::move(entry));
    return raw;
}

bool Engine::evaluate(const void* compiled, const EvalContext& ctx, Value& out) const {
    auto& impl = *m_impl;
    if (!compiled) return false;

    // functions e setadas uma vez na inicializacao e nao mudam depois, entao a
    // leitura sem lock aqui e segura; o cache por string e o que exige lock.
    std::string error;
    const bool ok = runInterpreter(static_cast<const ExprNode*>(compiled), ctx,
                                   impl.functions, out, &error);
    if (!ok) impl.lastError = std::move(error);
    return ok;
}

bool Engine::evaluateSource(const std::string& source, const EvalContext& ctx, Value& out,
                            std::string* error) {
    const void* compiled = compile(source, error);
    if (!compiled) return false;
    return evaluate(compiled, ctx, out);
}

std::string Engine::lastError() const {
    return m_impl->lastError;
}

void Engine::clearCache() {
    auto& impl = *m_impl;
    std::lock_guard lock(impl.mutex);
    impl.cache.clear();
}

size_t Engine::cacheSize() const {
    auto& impl = *m_impl;
    std::lock_guard lock(impl.mutex);
    return impl.cache.size();
}

void Engine::setFunction(const std::string& name, NativeFn fn) {
    auto& impl = *m_impl;
    std::lock_guard lock(impl.mutex);
    impl.functions[name] = std::move(fn);
}

std::shared_ptr<ExpressionBackend> Engine::backend() {
    // Ponte entre core::Property e este motor. O texto e compilado uma unica
    // vez (cache por string) e reavaliado a cada frame, que e o comportamento
    // esperado de uma expressao.
    struct Backend final : ExpressionBackend {
        bool evaluate(const Property& prop, const ExpressionContext& ctx, Value& out) override {
            EvalContext ec;
            ec.ctx = &ctx;
            ec.self = &prop;
            ec.resolveSibling = ctx.resolveSibling;
            return Engine::instance().evaluateSource(prop.expression(), ec, out, nullptr);
        }
    };
    static std::shared_ptr<ExpressionBackend> backend = std::make_shared<Backend>();
    return backend;
}

void installAsCoreBackend() {
    setExpressionBackend(Engine::instance().backend());
}

}  // namespace lmn::expr
