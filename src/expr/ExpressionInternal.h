// ExpressionInternal.h - Detalhes do lexer e do parser, compartilhados entre
// as duas unidades de traducao do motor de expressoes.
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Expression.h"

namespace lmn::expr {

enum class NodeKind : uint8_t {
    Number, String, Ident, Member, Index, Unary, Binary, Ternary, Call,
};

// No da arvore. Definido aqui (e nao no .cpp) porque lexer, parser e
// interpretador precisam dele; o cabecalho publico so o declara.
struct ExprNode {
    NodeKind kind = NodeKind::Number;
    double num = 0.0;
    std::string text;                // literal, identificador, membro ou operador
    std::vector<ExprPtr> children;
    int line = 0;
};

enum class Tok : uint8_t {
    End, Number, String, Ident,
    Plus, Minus, Star, Slash, Percent, Assign,
    Eq, Ne, Lt, Gt, Le, Ge,
    AndAnd, OrOr, Not,
    LParen, RParen, LBracket, RBracket,
    Comma, Dot, Colon, Question, Semicolon,
};

struct Token {
    Tok kind = Tok::End;
    std::string text;
    double num = 0.0;
    int line = 1;
    int col = 1;
};

class Lexer {
public:
    explicit Lexer(const std::string& src) : m_src(src) {}

    // Tokeniza tudo. Retorna false em erro e preenche error/line/col.
    bool tokenize(std::vector<Token>& out, std::string& error, int& line, int& col);

private:
    [[nodiscard]] char peek(size_t off = 0) const;
    char advance();
    bool match(char c);
    void skipTrivia();
    bool lexNumber(Token& t);
    bool lexString(Token& t);
    void lexIdent(Token& t);
    bool lexOperator(Token& t);

    const std::string& m_src;
    size_t m_pos = 0;
    int m_line = 1;
    int m_col = 1;
};

class Parser {
public:
    explicit Parser(std::vector<Token> toks) : m_toks(std::move(toks)) {}

    // Devolve nullptr em caso de erro (veja errorString()).
    ExprPtr parse();

    [[nodiscard]] bool failed() const { return !m_error.empty(); }
    [[nodiscard]] const std::string& errorString() const { return m_error; }
    [[nodiscard]] int errorLineValue() const { return m_errorLine; }
    [[nodiscard]] int errorColumnValue() const { return m_errorCol; }

private:
    [[nodiscard]] const Token& peek(size_t off = 0) const;
    const Token& next();
    bool accept(Tok k);
    void error(const std::string& msg);
    static ExprPtr make(NodeKind k, int line);
    static std::string dottedPath(const ExprNode* n);
    static int precedenceOf(Tok op);
    static char opChar(Tok op);
    bool acceptKeywordOp(Tok& out);

    ExprPtr parseTernary();
    ExprPtr parseBinary(int minPrec);
    ExprPtr parseUnary();
    ExprPtr parsePostfix();
    ExprPtr parsePrimary();
    bool parseArgs(ExprNode& call);

    std::vector<Token> m_toks;
    size_t m_pos = 0;
    std::string m_error;
    int m_errorLine = 0;
    int m_errorCol = 0;
};

// --- ruido deterministico -------------------------------------------------
// Usado por wiggle(). Nao mantem estado entre frames: renderizar o mesmo
// frame duas vezes produz exatamente o mesmo pixel e exportar em paralelo
// continua reproduzivel.
double hashToUnit(uint64_t seed, int64_t n);
double valueNoise(uint64_t seed, double t, int octave);
double fractalNoise(uint64_t seed, double t, int octaves);

// Avalia uma arvore ja compilada. Usado por Engine::evaluate.
bool runInterpreter(const ExprNode* root, const EvalContext& ctx,
                    const std::map<std::string, Engine::NativeFn>& fns,
                    Value& out, std::string* error);

}  // namespace lmn::expr
