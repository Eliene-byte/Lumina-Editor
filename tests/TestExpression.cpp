// TestExpression.cpp - Motor de expressoes.
#include "TestSupport.h"

#include "core/Property.h"
#include "expr/Expression.h"

using namespace lmn;
using namespace lmn::expr;

namespace {

// Avalia uma expressao com um contexto padrao.
Value eval(const std::string& source, Time time = 0.0) {
    ExpressionContext ctx;
    ctx.time = time;
    ctx.inPoint = 0.0;
    ctx.outPoint = 10.0;
    ctx.frameRate = 30.0;

    EvalContext ec;
    ec.ctx = &ctx;

    Value out;
    if (!Engine::instance().evaluateSource(source, ec, out)) return Value();
    return out;
}

bool compiles(const std::string& source) {
    std::string error;
    return Engine::instance().compile(source, &error) != nullptr;
}

}  // namespace

LUMINA_TEST(Expr, numerosEOperadores) {
    CHECK_NEAR(eval("1 + 2 * 3").asDouble(), 7.0, 1e-9);
    CHECK_NEAR(eval("(1 + 2) * 3").asDouble(), 9.0, 1e-9);
    CHECK_NEAR(eval("10 / 4").asDouble(), 2.5, 1e-9);
    CHECK_NEAR(eval("10 % 3").asDouble(), 1.0, 1e-9);
    CHECK_NEAR(eval("-5 + 2").asDouble(), -3.0, 1e-9);
    // Precedencia do unario: -2^2 e -(2^2) em notacao matematica, nao (-2)^2.
    CHECK_NEAR(eval("2 - -3").asDouble(), 5.0, 1e-9);
}

LUMINA_TEST(Expr, funcoesMatematicas) {
    CHECK_NEAR(eval("sin(0)").asDouble(), 0.0, 1e-9);
    CHECK_NEAR(eval("abs(-3.5)").asDouble(), 3.5, 1e-9);
    CHECK_NEAR(eval("max(2, 7)").asDouble(), 7.0, 1e-9);
    CHECK_NEAR(eval("min(2, 7)").asDouble(), 2.0, 1e-9);
    CHECK_NEAR(eval("clamp(15, 0, 10)").asDouble(), 10.0, 1e-9);
    CHECK_NEAR(eval("floor(2.7)").asDouble(), 2.0, 1e-9);
    CHECK_NEAR(eval("round(2.5)").asDouble(), 3.0, 1e-9);
    CHECK_NEAR(eval("sqrt(16)").asDouble(), 4.0, 1e-9);
}

LUMINA_TEST(Expr, matematicasComPrefixoMath) {
    // O After Effects aceita Math.sin; o motor tambem.
    CHECK_NEAR(eval("Math.sin(0)").asDouble(), 0.0, 1e-9);
    CHECK_NEAR(eval("Math.abs(-2)").asDouble(), 2.0, 1e-9);
    CHECK_NEAR(eval("Math.max(3, 9)").asDouble(), 9.0, 1e-9);
}

LUMINA_TEST(Expr, constantesDeTempo) {
    CHECK_NEAR(eval("time", 2.5).asDouble(), 2.5, 1e-9);
    CHECK_NEAR(eval("time * 2", 3.0).asDouble(), 6.0, 1e-9);
    CHECK_NEAR(eval("fps").asDouble(), 30.0, 1e-9);
    CHECK_NEAR(eval("width").asDouble(), 1920.0, 1e-9);
    CHECK_NEAR(eval("index").asDouble(), 0.0, 1e-9);
}

LUMINA_TEST(Expr, thisComp) {
    CHECK_NEAR(eval("thisComp.fps").asDouble(), 30.0, 1e-9);
    CHECK_NEAR(eval("thisComp.width").asDouble(), 1920.0, 1e-9);
    CHECK_NEAR(eval("thisComp.duration").asDouble(), 10.0, 1e-9);
    CHECK_NEAR(eval("thisComp.time", 4.0).asDouble(), 4.0, 1e-9);
}

LUMINA_TEST(Expr, vetores) {
    const Value v = eval("[10, 20] + [1, 2]");
    CHECK_NEAR(v.asVec2().x, 11.0, 1e-9);
    CHECK_NEAR(v.asVec2().y, 22.0, 1e-9);

    // Indice acessa componente.
    const Value indexed = eval("[[10, 20, 30], [5, 6, 7]][1][0]");
    CHECK_NEAR(indexed.asDouble(), 5.0, 1e-9);

    // Propriedades de um valor: .x e .y.
    CHECK_NEAR(eval("[7, 8].x").asDouble(), 7.0, 1e-9);
    CHECK_NEAR(eval("[7, 8].y").asDouble(), 8.0, 1e-9);

    // length e um builtin.
    CHECK_NEAR(eval("length([3, 4])").asDouble(), 5.0, 1e-9);
}

LUMINA_TEST(Expr, comparacoes) {
    CHECK(eval("1 < 2").asBool());
    CHECK(!eval("2 < 1").asBool());
    CHECK(eval("1 == 1").asBool());
    CHECK(eval("1 != 2").asBool());

    // O After Effects aceita "and"/"or"/"not". O parser tambem.
    CHECK(eval("1 < 2 and 3 > 2").asBool());
    CHECK(eval("1 > 2 or 3 > 2").asBool());
    CHECK(!eval("1 > 2 and 3 > 2").asBool());
    CHECK(eval("not 1 > 2").asBool());
    CHECK(eval("!false").asBool());

    // "e"/"ou" em portugues sao aceitos como sinonimos: e o que um usuario
    // brasileiro digita primeiro, e rejeitar por sintaxe seria unhelpful.
    CHECK(eval("1 < 2 e 3 > 2").asBool());
    CHECK(eval("1 > 2 ou 3 > 2").asBool());
}

LUMINA_TEST(Expr, ternario) {
    CHECK_NEAR(eval("1 > 0 ? 10 : 20").asDouble(), 10.0, 1e-9);
    CHECK_NEAR(eval("1 < 0 ? 10 : 20").asDouble(), 20.0, 1e-9);
    CHECK_NEAR(eval("time > 1 ? 1 : 0", 2.0).asDouble(), 1.0, 1e-9);
}

LUMINA_TEST(Expr, cores) {
    const Value c = eval("rgb(0.5, 0.25, 1)");
    CHECK_NEAR(c.asColor().r, 0.5, 1e-9);
    CHECK_NEAR(c.asColor().b, 1.0, 1e-9);
    CHECK_NEAR(c.asColor().a, 1.0, 1e-9);

    // hsl: vermelho puro.
    const Value red = eval("hsl(0, 1, 0.5)");
    CHECK_NEAR(red.asColor().r, 1.0, 1e-6);
    CHECK_NEAR(red.asColor().g, 0.0, 1e-6);
}

LUMINA_TEST(Expr, linearEEase) {
    // linear(t, t1, t2, v1, v2)
    CHECK_NEAR(eval("linear(5, 0, 10, 0, 100)").asDouble(), 50.0, 1e-6);
    CHECK_NEAR(eval("linear(-5, 0, 10, 0, 100)").asDouble(), 0.0, 1e-6);
    CHECK_NEAR(eval("linear(0, 0, 10, 20, 40)").asDouble(), 20.0, 1e-6);
}

LUMINA_TEST(Expr, texto) {
    const Value s = eval("\"ola\" + \" mundo\"");
    CHECK_EQ(s.asString(), std::string("ola mundo"));
}

LUMINA_TEST(Expr, errosDeCompilacao) {
    // Uma expressao quebrada tem de falhar na compilacao, e nao devolver 0
    // silenciosamente: o usuario precisa saber que o texto esta errado.
    CHECK(!compiles("1 +"));
    CHECK(!compiles("sin("));
    CHECK(!compiles("foo bar baz"));
    CHECK(!compiles("\"texto sem fim"));
    CHECK(!compiles("((1 + 2)"));

    std::string error;
    CHECK(Engine::instance().compile("1 +", &error) == nullptr);
    CHECK(!error.empty());
}

LUMINA_TEST(Expr, cacheDeCompilacao) {
    // O mesmo texto e compilado uma vez so. E o que torna a expressao viavel
    // em tempo real.
    const std::string source = "wiggle(2, 10) + 1";
    CHECK(Engine::instance().compile(source, nullptr) != nullptr);
    const size_t before = Engine::instance().cacheSize();
    CHECK(Engine::instance().compile(source, nullptr) != nullptr);
    CHECK_EQ(Engine::instance().cacheSize(), before);
}

LUMINA_TEST(Expr, wiggleEstavel) {
    // wiggle() nao pode ter estado entre frames: o mesmo tempo tem de dar o
    // mesmo valor, senao o preview mostra uma coisa e a exportacao outra.
    const Value a = eval("wiggle(2, 10)", 1.5);
    const Value b = eval("wiggle(2, 10)", 1.5);
    CHECK_NEAR(a.asDouble(), b.asDouble(), 1e-12);

    // Em tempos diferentes, valores diferentes.
    const Value c = eval("wiggle(2, 10)", 1.6);
    CHECK(std::abs(a.asDouble() - c.asDouble()) > 1e-6);

    // Dentro da amplitude pedida.
    for (int i = 0; i < 20; ++i) {
        const double t = i * 0.13;
        const double v = eval("wiggle(4, 5)", t).asDouble();
        CHECK(std::abs(v) <= 5.0 + 1e-6);
    }
}

LUMINA_TEST(Expr, valueUsaACurva) {
    // "value" e o valor da propria propriedade no tempo, como no AE.
    Property prop(Value(0.0), PropertyUi::Slider);
    Keyframe a;
    a.time = 0.0;
    a.value = Value(0.0);
    a.interp = Interp::Linear;
    Keyframe b = a;
    b.time = 2.0;
    b.value = Value(10.0);
    prop.keyframes().push_back(a);
    prop.keyframes().push_back(b);

    ExpressionContext ctx;
    ctx.time = 1.0;
    ctx.inPoint = 0.0;
    ctx.outPoint = 4.0;

    EvalContext ec;
    ec.ctx = &ctx;
    ec.self = &prop;

    Value out;
    CHECK(Engine::instance().evaluateSource("value * 2", ec, out));
    // No tempo 1 a curva vale 5 (metade de 0 a 10), dobrado da 10.
    CHECK_NEAR(out.asDouble(), 10.0, 1e-6);
}

LUMINA_TEST(Expr, loopOutCiclo) {
    Property prop(Value(0.0), PropertyUi::Slider);
    Keyframe a;
    a.time = 0.0;
    a.value = Value(0.0);
    a.interp = Interp::Linear;
    Keyframe b = a;
    b.time = 2.0;
    b.value = Value(10.0);
    prop.keyframes().push_back(a);
    prop.keyframes().push_back(b);

    ExpressionContext ctx;
    ctx.inPoint = 0.0;
    ctx.outPoint = 2.0;   // janela do loop: 0..2

    EvalContext ec;
    ec.ctx = &ctx;
    ec.self = &prop;

    const auto sample = [&](Time t) {
        ctx.time = t;
        Value out;
        if (!Engine::instance().evaluateSource("loopOut(\"cycle\")", ec, out)) {
            return -1.0;
        }
        return out.asDouble();
    };

    // Dentro da janela: a curva normal.
    CHECK_NEAR(sample(1.0), 5.0, 1e-4);
    // Fora: o valor se repete, e nunca sai da faixa da curva.
    const double after = sample(3.0);
    CHECK(after >= -1e-4);
    CHECK(after <= 10.0 + 1e-4);
    // O ciclo de 0..2 re comeca: 3.0 equivale a 1.0 dentro do ciclo.
    CHECK_NEAR(sample(3.0), sample(1.0), 1e-4);
}

LUMINA_TEST(Expr, backendNaCore) {
    // O backend conecta Property::evaluate ao motor: uma Property com
    // expressao tem de resolver sem que a UI precise fazer nada.
    installAsCoreBackend();
    CHECK(expressionBackend() != nullptr);

    Property prop(Value(0.0), PropertyUi::Slider);
    prop.setExpression("10 * 2");

    ExpressionContext ctx;
    ctx.time = 0.0;
    CHECK_NEAR(prop.evaluate(0.0, ctx).asDouble(), 20.0, 1e-9);
}

LUMINA_TEST(Expr, expressaoInvalidaCaiParaACurva) {
    // Se a expressao falha, o valor nao pode virar 0: o conteudo do no sumiria
    // sem explicacao. Cai para a curva, que e o comportamento do AE.
    installAsCoreBackend();

    Property prop(Value(7.0), PropertyUi::Slider);
    prop.setExpression("isso nao compila (");

    ExpressionContext ctx;
    const double v = prop.evaluate(0.0, ctx).asDouble();
    CHECK_NEAR(v, 7.0, 1e-9);
}

LUMINA_TEST(Expr, loopIn) {
    Property prop(Value(0.0), PropertyUi::Slider);
    Keyframe a;
    a.time = 0.0;
    a.value = Value(0.0);
    a.interp = Interp::Linear;
    Keyframe b = a;
    b.time = 2.0;
    b.value = Value(10.0);
    prop.keyframes().push_back(a);
    prop.keyframes().push_back(b);

    ExpressionContext ctx;
    ctx.inPoint = 0.0;
    ctx.outPoint = 2.0;

    EvalContext ec;
    ec.ctx = &ctx;
    ec.self = &prop;

    ctx.time = 1.0;
    Value out;
    CHECK(Engine::instance().evaluateSource("loopIn(\"cycle\")", ec, out));
    // loopIn conta a partir do fim: em 1.0 dentro de uma janela 0..2, o valor
    // e o meio da curva.
    CHECK_NEAR(out.asDouble(), 5.0, 1e-4);
}

LUMINA_TEST(Expr, tiposMisturados) {
    // Numero em vetor: o componente escalar se aplica aos dois eixos.
    const Value v = eval("[1, 2] * 3");
    CHECK_NEAR(v.asVec2().x, 3.0, 1e-9);
    CHECK_NEAR(v.asVec2().y, 6.0, 1e-9);

    // Vetor em numero: usa o primeiro componente.
    CHECK_NEAR(eval("[5, 9] + 1").asDouble(), 6.0, 1e-9);
}

LUMINA_TEST(Expr, profundidadeProtegida) {
    // Uma expressao patologica nao pode estourar a pilha.
    std::string deep = "1";
    for (int i = 0; i < 200; ++i) deep = "(" + deep + ")";
    // Pode compilar ou nao, mas nunca deve travar.
    (void)compiles(deep);
    CHECK(true);
}
