#include "ExpressionInternal.h"

namespace lmn::expr {

namespace {

struct OpInfo {
    Tok tok;
    int prec;
};

const std::vector<OpInfo>& opTable() {
    static const std::vector<OpInfo> table = {
        {Tok::OrOr, 1},
        {Tok::AndAnd, 2},
        {Tok::Eq, 3}, {Tok::Ne, 3},
        {Tok::Lt, 4}, {Tok::Gt, 4}, {Tok::Le, 4}, {Tok::Ge, 4},
        {Tok::Plus, 5}, {Tok::Minus, 5},
        {Tok::Star, 6}, {Tok::Slash, 6}, {Tok::Percent, 6},
    };
    return table;
}

}  // namespace

LoopType loopTypeFromName(const std::string& s) noexcept {
    if (s == "cycle")    return LoopType::Cycle;
    if (s == "pingpong") return LoopType::PingPong;
    if (s == "continue") return LoopType::Continue;
    if (s == "offset")   return LoopType::Offset;
    return LoopType::None;
}

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

const Token& Parser::peek(size_t off) const {
    const size_t i = m_pos + off;
    return i < m_toks.size() ? m_toks[i] : m_toks.back();
}

const Token& Parser::next() {
    const Token& t = peek();
    if (m_pos + 1 < m_toks.size()) ++m_pos;
    return t;
}

bool Parser::accept(Tok k) {
    if (peek().kind != k) return false;
    next();
    return true;
}

void Parser::error(const std::string& msg) {
    if (!m_error.empty()) return;
    m_error = msg;
    m_errorLine = peek().line;
    m_errorCol = peek().col;
}

ExprPtr Parser::make(NodeKind k, int line) {
    auto n = std::make_unique<ExprNode>();
    n->kind = k;
    n->line = line;
    return n;
}

int Parser::precedenceOf(Tok op) {
    for (const auto& o : opTable()) {
        if (o.tok == op) return o.prec;
    }
    return -1;
}

char Parser::opChar(Tok op) {
    switch (op) {
        case Tok::OrOr:    return '|';
        case Tok::AndAnd:  return '&';
        case Tok::Eq:      return '=';
        case Tok::Ne:      return '!';
        case Tok::Lt:      return '<';
        case Tok::Gt:      return '>';
        case Tok::Le:      return '<';
        case Tok::Ge:      return '>';
        case Tok::Plus:    return '+';
        case Tok::Minus:   return '-';
        case Tok::Star:    return '*';
        case Tok::Slash:   return '/';
        case Tok::Percent: return '%';
        default:           return '?';
    }
}

bool Parser::acceptKeywordOp(Tok& out) {
    if (peek().kind != Tok::Ident) return false;
    const std::string& id = peek().text;
    // Ingles do After Effects e portugues: um usuario que escreve em portugues
    // digita "e"/"ou" antes de "and"/"or", e recusar seria mais unhelpful que
    // util. "e" e uma palavra muito comum em texto, entao so e operador entre
    // dois operandos - o contexto do parser ja garante isso.
    if (id == "and" || id == "e")        out = Tok::AndAnd;
    else if (id == "or" || id == "ou")   out = Tok::OrOr;
    else if (id == "not" || id == "nao") out = Tok::Not;
    else return false;
    next();
    return true;
}

std::string Parser::dottedPath(const ExprNode* n) {
    if (!n) return {};
    if (n->kind == NodeKind::Ident) return n->text;
    if (n->kind == NodeKind::Member) {
        const std::string base = dottedPath(n->children[0].get());
        return base.empty() ? n->text : base + "." + n->text;
    }
    return {};
}

ExprPtr Parser::parse() {
    ExprPtr root = parseTernary();
    if (!root || failed()) return nullptr;

    // Varias sentencas separadas por ';': vale a ultima.
    while (peek().kind == Tok::Semicolon) {
        next();
        root = parseTernary();
        if (!root || failed()) return nullptr;
    }
    if (peek().kind != Tok::End) {
        error("tokens inesperados no fim da expressao");
        return nullptr;
    }
    return root;
}

ExprPtr Parser::parseTernary() {
    ExprPtr cond = parseBinary(1);
    if (!cond || failed()) return cond;
    if (peek().kind != Tok::Question) return cond;

    const int line = peek().line;
    next();
    auto node = make(NodeKind::Ternary, line);
    node->children.push_back(std::move(cond));
    node->children.push_back(parseTernary());
    if (!node->children[1] || failed()) return nullptr;
    if (!accept(Tok::Colon)) {
        error("esperado ':' no ternario");
        return nullptr;
    }
    node->children.push_back(parseTernary());
    return failed() ? nullptr : std::move(node);
}

ExprPtr Parser::parseBinary(int minPrec) {
    ExprPtr lhs = parseUnary();
    if (!lhs || failed()) return lhs;

    while (true) {
        int prec = precedenceOf(peek().kind);
        if (prec < 0) {
            Tok kw = Tok::End;
            if (!acceptKeywordOp(kw)) break;
            prec = precedenceOf(kw);
            if (prec < 0) break;  // "not" como palavra-chave: operador unario
        }
        if (prec < minPrec) break;

        const char opc = opChar(peek().kind);
        const int line = peek().line;
        next();
        ExprPtr rhs = parseBinary(prec + 1);
        if (!rhs) {
            error("operador sem operando a direita");
            return nullptr;
        }
        auto node = make(NodeKind::Binary, line);
        node->text = std::string(1, opc);
        node->children.push_back(std::move(lhs));
        node->children.push_back(std::move(rhs));
        lhs = std::move(node);
    }
    return lhs;
}

ExprPtr Parser::parseUnary() {
    const Tok k = peek().kind;
    if (k == Tok::Not ||
        (k == Tok::Ident && (peek().text == "not" || peek().text == "nao"))) {
        const int line = peek().line;
        next();
        auto node = make(NodeKind::Unary, line);
        node->text = "!";
        node->children.push_back(parseUnary());
        return node;
    }
    if (k == Tok::Plus) {
        next();
        return parseUnary();
    }
    if (k == Tok::Minus) {
        const int line = peek().line;
        next();
        auto node = make(NodeKind::Unary, line);
        node->text = "-";
        node->children.push_back(parseUnary());
        return node;
    }
    return parsePostfix();
}

ExprPtr Parser::parsePostfix() {
    ExprPtr base = parsePrimary();
    if (!base || failed()) return base;

    while (true) {
        if (peek().kind == Tok::Dot) {
            const int line = peek().line;
            next();
            if (peek().kind != Tok::Ident && peek().kind != Tok::Number) {
                error("esperado um nome de propriedade apos '.'");
                return nullptr;
            }
            const std::string name = next().text;

            // Nome com ponto seguido de '(' e uma chamada: Math.sin(x),
            // effect("Blur").Amount(x). O nome completo e montado aqui.
            if (peek().kind == Tok::LParen) {
                const std::string prefix = dottedPath(base.get());
                auto call = make(NodeKind::Call, line);
                call->text = prefix.empty() ? name : prefix + "." + name;
                next();
                if (!parseArgs(*call)) return nullptr;
                base = std::move(call);
                continue;
            }

            auto node = make(NodeKind::Member, line);
            node->text = name;
            node->children.push_back(std::move(base));
            base = std::move(node);
            continue;
        }
        if (peek().kind == Tok::LBracket) {
            const int line = peek().line;
            next();
            auto node = make(NodeKind::Index, line);
            node->children.push_back(std::move(base));
            node->children.push_back(parseTernary());
            if (!node->children[1] || failed()) return nullptr;
            if (!accept(Tok::RBracket)) {
                error("esperado ']'");
                return nullptr;
            }
            base = std::move(node);
            continue;
        }
        break;
    }
    return base;
}

bool Parser::parseArgs(ExprNode& call) {
    if (accept(Tok::RParen)) return true;
    while (true) {
        ExprPtr arg = parseTernary();
        if (!arg || failed()) return false;
        call.children.push_back(std::move(arg));
        if (accept(Tok::Comma)) continue;
        if (!accept(Tok::RParen)) {
            error("esperado ')' ao fim da chamada");
            return false;
        }
        return true;
    }
}

ExprPtr Parser::parsePrimary() {
    const Token& t = peek();
    switch (t.kind) {
        case Tok::Number: {
            auto n = make(NodeKind::Number, t.line);
            n->num = t.num;
            n->text = t.text;
            next();
            return n;
        }
        case Tok::String: {
            auto n = make(NodeKind::String, t.line);
            n->text = t.text;
            next();
            return n;
        }
        case Tok::Ident: {
            const int line = t.line;
            const std::string name = t.text;
            next();
            if (peek().kind == Tok::LParen) {
                auto call = make(NodeKind::Call, line);
                call->text = name;
                next();
                if (!parseArgs(*call)) return nullptr;
                return call;
            }
            auto n = make(NodeKind::Ident, line);
            n->text = name;
            return n;
        }
        case Tok::LParen: {
            next();
            ExprPtr inner = parseTernary();
            if (!inner || failed()) return nullptr;
            if (!accept(Tok::RParen)) {
                error("esperado ')'");
                return nullptr;
            }
            return inner;
        }
        case Tok::End:
            error("expressao incompleta");
            return nullptr;
        default:
            error("token inesperado");
            return nullptr;
    }
}

}  // namespace lmn::expr
