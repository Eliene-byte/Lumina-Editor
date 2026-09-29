// TestShaders.cpp - Valida que os shaders GLSL compilam.
//
// Um shader com erro de sintaxe e o tipo de falha mais caro do projeto: o
// programa compila, o editor abre, e o no simplesmente nao faz nada. Rodar o
// compilador de verdade no CI e o unico jeito de pegar isso antes do usuario.
#include "TestSupport.h"

#include <QFile>
#include <QString>
#include <QStringList>

#include <set>

#include "gpu/ShaderLoader.h"
#include "gpu/ShaderProgram.h"

using namespace lmn;
using namespace lmn::gpu;

namespace {

// Lista de shaders que precisam existir. Mantida a mao de proposito: se um
// shader for removido, o teste tem de falhar, e nao pular em silencio.
const char* const kRequiredShaders[] = {
    "common.glsl",     "copy.frag",          "solid.frag",
    "merge.frag",      "blur.frag",          "view_transform.frag",
    "brightness_contrast.frag",               "curves.frag",
    "hue_saturation.frag",                     "color_balance.frag",
    "glow.frag",       "shape.frag",         "noise_texture.frag",
    "displace.frag",   "channel_key.frag",   "lut3d.frag",
    "matte.frag",      "checkerboard.frag",  "vignette.frag",
    "set_alpha.frag",  "time_stretch.frag",
};

}  // namespace

LUMINA_TEST(Shaders, todosPresentes) {
    for (const char* name : kRequiredShaders) {
        const QString path = QStringLiteral(":/shaders/") + QLatin1String(name);
        if (!QFile::exists(path)) {
            ::lmn::test::fail(__FILE__, __LINE__,
                              "shader ausente: " + std::string(name));
        }
    }
}

LUMINA_TEST(Shaders, includesResolvidos) {
    // O include e resolvido em tempo de execucao por ShaderLoader. Se a
    // resolucao falhar, o fonte final fica sem as funcoes de common.glsl e o
    // compilador vai reclamar de "luma" indefinido - mas so no shader.
    for (const char* name : kRequiredShaders) {
        const QString path = QStringLiteral(":/shaders/") + QLatin1String(name);
        const QString source = ShaderLoader::loadFragment(path);
        if (source.isEmpty()) {
            ::lmn::test::fail(__FILE__, __LINE__,
                              "falha ao ler: " + std::string(name));
        }
    }
}

LUMINA_TEST(Shaders, versaoNoTopo) {
    // A diretiva #version tem de ser a primeira linha, senao o compilador
    // ignora e o shader roda como 1.10.
    for (const char* name : kRequiredShaders) {
        if (std::string(name).find(".glsl") != std::string::npos) continue;
        const QString path = QStringLiteral(":/shaders/") + QLatin1String(name);
        const QString source = ShaderLoader::loadFragment(path);
        const QStringList lines = source.split(QLatin1Char('\n'));
        if (lines.isEmpty() || !lines.first().startsWith(QLatin1String("#version"))) {
            ::lmn::test::fail(__FILE__, __LINE__,
                              std::string(name) + ": #version nao esta na primeira linha");
        }
        CHECK(!lines.isEmpty());
        CHECK(lines.first().startsWith(QLatin1String("#version")));
    }
}

LUMINA_TEST(Shaders, declaraSaida) {
    // Todo fragment precisa declarar a saida e escrever nela. Um shader que
    // calcula e esquece de atribuir produz preto, sem erro de compilacao.
    for (const char* name : kRequiredShaders) {
        if (std::string(name).find(".glsl") != std::string::npos) continue;
        const QString path = QStringLiteral(":/shaders/") + QLatin1String(name);
        const QString source = ShaderLoader::loadFragment(path);

        if (!source.contains(QLatin1String("out vec4 fragColor"))) {
            ::lmn::test::fail(__FILE__, __LINE__,
                              std::string(name) + ": sem 'out vec4 fragColor'");
        }
        if (!source.contains(QLatin1String("fragColor ="))) {
            ::lmn::test::fail(__FILE__, __LINE__,
                              std::string(name) + ": fragColor nunca e atribuido");
        }
    }
}

LUMINA_TEST(Shaders, mainEmTodos) {
    for (const char* name : kRequiredShaders) {
        if (std::string(name).find(".glsl") != std::string::npos) continue;
        const QString path = QStringLiteral(":/shaders/") + QLatin1String(name);
        const QString source = ShaderLoader::loadFragment(path);
        if (!source.contains(QLatin1String("void main"))) {
            ::lmn::test::fail(__FILE__, __LINE__,
                              std::string(name) + ": sem ponto de entrada main");
        }
    }
}

LUMINA_TEST(Shaders, semIncludeSolto) {
    // Nenhum #include pode sobrar depois da resolucao: o GLSL nao entende a
    // diretiva e o compilador daria erro de sintaxe.
    for (const char* name : kRequiredShaders) {
        const QString path = QStringLiteral(":/shaders/") + QLatin1String(name);
        const QString source = ShaderLoader::loadFragment(path);
        if (source.contains(QLatin1String("#include"))) {
            ::lmn::test::fail(__FILE__, __LINE__,
                              std::string(name) + ": #include nao foi resolvido");
        }
    }
}

LUMINA_TEST(Shaders, balancedChaves) {
    // Uma chave desbalanceada e o erro de shader mais comum depois de
    // escrever rapido. Contar e mais barato que compilar.
    for (const char* name : kRequiredShaders) {
        if (std::string(name).find(".glsl") != std::string::npos) continue;
        const QString path = QStringLiteral(":/shaders/") + QLatin1String(name);
        const QString source = ShaderLoader::loadFragment(path);

        int braces = 0;
        int parens = 0;
        for (const QChar c : source) {
            if (c == QLatin1Char('{')) ++braces;
            else if (c == QLatin1Char('}')) --braces;
            else if (c == QLatin1Char('(')) ++parens;
            else if (c == QLatin1Char(')')) --parens;

            if (braces < 0 || parens < 0) break;
        }
        if (braces != 0) {
            ::lmn::test::fail(__FILE__, __LINE__,
                              std::string(name) + ": chaves desbalanceadas (" +
                                  std::to_string(braces) + ")");
        }
        if (parens != 0) {
            ::lmn::test::fail(__FILE__, __LINE__,
                              std::string(name) + ": parenteses desbalanceados (" +
                                  std::to_string(parens) + ")");
        }
    }
}
