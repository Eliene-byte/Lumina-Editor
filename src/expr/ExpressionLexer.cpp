#include "ExpressionInternal.h"

#include <cctype>
#include <cstdlib>

namespace lmn::expr {

// ---------------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------------

char Lexer::peek(size_t off) const {
    return m_pos + off < m_src.size() ? m_src[m_pos + off] : '\0';
}

char Lexer::advance() {
    const char c = m_src[m_pos++];
    if (c == '\n') {
        ++m_line;
        m_col = 1;
    } else {
        ++m_col;
    }
    return c;
}

bool Lexer::match(char c) {
    if (peek() != c) return false;
    advance();
    return true;
}

void Lexer::skipTrivia() {
    while (m_pos < m_src.size()) {
        const char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
            continue;
        }
        // Comentarios: // ate o fim da linha, /* ... */ e # ate o fim da linha
        // (Python, que a maioria dos motion designers ja usa).
        if (c == '/' && peek(1) == '/') {
            while (m_pos < m_src.size() && peek() != '\n') advance();
            continue;
        }
        if (c == '/' && peek(1) == '*') {
            advance();
            advance();
            while (m_pos < m_src.size() && !(peek() == '*' && peek(1) == '/')) advance();
            if (m_pos < m_src.size()) {
                advance();
                advance();
            }
            continue;
        }
        if (c == '#') {
            while (m_pos < m_src.size() && peek() != '\n') advance();
            continue;
        }
        break;
    }
}

bool Lexer::lexNumber(Token& t) {
    const size_t start = m_pos;

    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
        advance();
        advance();
        while (std::isxdigit(static_cast<unsigned char>(peek()))) advance();
        t.text = m_src.substr(start, m_pos - start);
        t.num = static_cast<double>(std::strtoull(t.text.c_str() + 2, nullptr, 16));
        t.kind = Tok::Number;
        return true;
    }

    bool seenDot = false;
    bool seenExp = false;
    while (m_pos < m_src.size()) {
        const char c = peek();
        if (std::isdigit(static_cast<unsigned char>(c))) {
            advance();
            continue;
        }
        if (c == '.' && !seenDot && !seenExp) {
            seenDot = true;
            advance();
            continue;
        }
        if ((c == 'e' || c == 'E') && !seenExp && m_pos + 1 < m_src.size() &&
            (std::isdigit(static_cast<unsigned char>(peek(1))) || peek(1) == '-' ||
             peek(1) == '+')) {
            seenExp = true;
            advance();
            advance();
            continue;
        }
        break;
    }
    t.text = m_src.substr(start, m_pos - start);
    t.num = std::strtod(t.text.c_str(), nullptr);
    t.kind = Tok::Number;
    return true;
}

bool Lexer::lexString(Token& t) {
    const char quote = advance();
    std::string s;
    while (m_pos < m_src.size() && peek() != quote) {
        const char c = advance();
        if (c == '\\' && m_pos < m_src.size()) {
            const char e = advance();
            switch (e) {
                case 'n':  s += '\n'; break;
                case 't':  s += '\t'; break;
                case 'r':  s += '\r'; break;
                case '\\': s += '\\'; break;
                case '"':  s += '"';  break;
                case '\'': s += '\''; break;
                default:   s += e;    break;
            }
        } else {
            s += c;
        }
    }
    if (m_pos >= m_src.size()) return false;  // aspas nao fechadas
    advance();
    t.kind = Tok::String;
    t.text = std::move(s);
    return true;
}

void Lexer::lexIdent(Token& t) {
    const size_t start = m_pos;
    while (m_pos < m_src.size()) {
        const char c = peek();
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
            advance();
        } else {
            break;
        }
    }
    t.kind = Tok::Ident;
    t.text = m_src.substr(start, m_pos - start);
}

bool Lexer::lexOperator(Token& t) {
    const char c = peek();
    switch (c) {
        case '+': t.kind = Tok::Plus;     break;
        case '-': t.kind = Tok::Minus;    break;
        case '*': t.kind = Tok::Star;     break;
        case '/': t.kind = Tok::Slash;    break;
        case '%': t.kind = Tok::Percent;  break;
        case '(': t.kind = Tok::LParen;   break;
        case ')': t.kind = Tok::RParen;   break;
        case '[': t.kind = Tok::LBracket; break;
        case ']': t.kind = Tok::RBracket; break;
        case ',': t.kind = Tok::Comma;    break;
        case '.': t.kind = Tok::Dot;      break;
        case ':': t.kind = Tok::Colon;    break;
        case '?': t.kind = Tok::Question; break;
        case ';': t.kind = Tok::Semicolon; break;
        case '=': t.kind = match('=') ? Tok::Eq : Tok::Assign; break;
        case '!': t.kind = match('=') ? Tok::Ne : Tok::Not;   break;
        case '<': t.kind = match('=') ? Tok::Le : Tok::Lt;    break;
        case '>': t.kind = match('=') ? Tok::Ge : Tok::Gt;    break;
        case '&': if (!match('&')) { advance(); return false; } t.kind = Tok::AndAnd; break;
        case '|': if (!match('|')) { advance(); return false; } t.kind = Tok::OrOr;   break;
        default: advance(); return false;
    }
    advance();
    t.text = std::string(1, c);
    return true;
}

bool Lexer::tokenize(std::vector<Token>& out, std::string& error, int& line, int& col) {
    while (true) {
        skipTrivia();
        if (m_pos >= m_src.size()) {
            out.push_back(Token{Tok::End, {}, 0.0, m_line, m_col});
            return true;
        }

        Token t;
        t.line = m_line;
        t.col = m_col;
        const char c = peek();

        const bool startsNumber =
            std::isdigit(static_cast<unsigned char>(c)) != 0 ||
            (c == '.' && m_pos + 1 < m_src.size() &&
             std::isdigit(static_cast<unsigned char>(m_src[m_pos + 1])) != 0);

        bool ok = true;
        if (startsNumber) {
            ok = lexNumber(t);
        } else if (c == '"' || c == '\'') {
            ok = lexString(t);
        } else if (std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '$') {
            lexIdent(t);
        } else {
            ok = lexOperator(t);
        }

        if (!ok) {
            error = (c == '"' || c == '\'') ? "texto sem fim"
                                            : std::string("caractere inesperado '") + c + "'";
            line = t.line;
            col = t.col;
            return false;
        }
        out.push_back(std::move(t));
    }
}

// ---------------------------------------------------------------------------
// Ruido
// ---------------------------------------------------------------------------

double hashToUnit(uint64_t seed, int64_t n) {
    uint64_t h = seed * 0x9E3779B97F4A7C15ull ^
                 (static_cast<uint64_t>(n) * 0xBF58476D1CE4E5B9ull);
    h ^= h >> 30;
    h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 27;
    h *= 0x94D049BB133111EBull;
    h ^= h >> 31;
    return static_cast<double>(h >> 11) / static_cast<double>(1ull << 53);
}

double valueNoise(uint64_t seed, double t, int octave) {
    const double s = t * (1.0 + octave);
    const double i = std::floor(s);
    const double f = s - i;
    const auto i0 = static_cast<int64_t>(i);
    const uint64_t k = seed + static_cast<uint64_t>(octave) * 7919ull;
    const double a = hashToUnit(k, i0);
    const double b = hashToUnit(k, i0 + 1);
    return (a + (b - a) * smoothStepEasing(f)) * 2.0 - 1.0;
}

double fractalNoise(uint64_t seed, double t, int octaves) {
    double sum = 0.0, amp = 1.0, norm = 0.0;
    for (int o = 0; o < octaves; ++o) {
        sum += valueNoise(seed, t, o) * amp;
        norm += amp;
        amp *= 0.5;
    }
    return norm > 0.0 ? sum / norm : 0.0;
}

}  // namespace lmn::expr
