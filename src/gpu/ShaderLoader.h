// ShaderLoader.h - Carrega shaders do recurso Qt resolvendo #include.
//
// GLSL nao tem include, e usar a extensao GL_GOOGLE_include_directive nao e
// garantido em drivers de Windows 10 antigo. Resolver aqui custa pouco e
// mantem os shaders organized em arquivos.
#pragma once

#include <QString>
#include <unordered_map>
#include <vector>

#include "ShaderProgram.h"

namespace lmn::gpu {

class ShaderLoader {
public:
    // Le ":/shaders/merge.frag", substitui #include "common.glsl" e devolve o
    // fonte final. Devolve string vazia se o arquivo nao existir.
    [[nodiscard]] static QString loadFragment(const QString& resourcePath);

    // Le um recurso e resolve includes recursivamente.
    [[nodiscard]] static QString resolve(const QString& resourcePath,
                                         std::unordered_map<std::string, QString>* cache);

    // Limpa o cache de fontes (usado em testes).
    static void clearCache();
};

}  // namespace lmn::gpu
