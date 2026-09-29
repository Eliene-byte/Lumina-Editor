// TestSupport.h - Utilidades compartilhadas pelos testes.
//
// Um runner proprio em vez de Qt Test: os testes precisam rodar em um
// processo sem display (o CI nao tem servidor X11), e Qt Test exige
// QApplication. O runner e minimo de proposito - 80 linhas em vez de uma
// dependencia que so os testes usariam.
#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

namespace lmn::test {

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void()> body;
};

class Registry {
public:
    static Registry& instance() {
        static Registry r;
        return r;
    }

    void add(TestCase test) { m_tests.push_back(std::move(test)); }
    [[nodiscard]] const std::vector<TestCase>& tests() const { return m_tests; }

private:
    std::vector<TestCase> m_tests;
};

struct Registrar {
    Registrar(const char* suite, const char* name, std::function<void()> body) {
        Registry::instance().add(TestCase{suite, name, std::move(body)});
    }
};

class Failure {
public:
    explicit Failure(std::string message) : m_message(std::move(message)) {}
    [[nodiscard]] const std::string& message() const { return m_message; }

private:
    std::string m_message;
};

[[noreturn]] void fail(const char* file, int line, const std::string& message);
int runAll(int argc, char** argv);

inline bool nearlyEqual(double a, double b, double tolerance = 1e-6) {
    return std::abs(a - b) <= tolerance;
}

}  // namespace lmn::test

#define LUMINA_TEST(suite, name)                                                 \
    static void suite##_##name();                                               \
    static ::lmn::test::Registrar reg_##suite##_##name(#suite, #name,           \
                                                       suite##_##name);          \
    static void suite##_##name()

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) ::lmn::test::fail(__FILE__, __LINE__, "CHECK falhou: " #cond); \
    } while (0)

#define CHECK_EQ(a, b)                                                           \
    do {                                                                         \
        const auto lhs_ = (a);                                                   \
        const auto rhs_ = (b);                                                   \
        if (!(lhs_ == rhs_)) {                                                   \
            ::lmn::test::fail(__FILE__, __LINE__,                                \
                              std::string("CHECK_EQ falhou: " #a " == " #b));    \
        }                                                                        \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                    \
    do {                                                                         \
        const double lhs_ = (a);                                                 \
        const double rhs_ = (b);                                                 \
        if (!::lmn::test::nearlyEqual(lhs_, rhs_, tol)) {                        \
            ::lmn::test::fail(__FILE__, __LINE__,                                \
                              std::string("CHECK_NEAR falhou: " #a " != " #b)); \
        }                                                                        \
    } while (0)
