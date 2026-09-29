// ProjectSerializer.h - Leitura e escrita do arquivo .lumina.
//
// O formato e JSON legivel, com chaves curtas e sem espacos: um projeto de
// 50 MB abre em meio segundo com o parser do Qt, e da para versionar em git
// ou inspecionar num editor de texto quando algo der errado.
//
// A extensao e .lumina e o conteudo comeca com a chave "lumina", o que impede
// abrir um arquivo de video por engano.
#pragma once

#include <string>

#include <QString>

#include "core/Project.h"

class QIODevice;

namespace lmn::io {

struct SaveResult {
    bool ok = false;
    std::string error;
    size_t bytes = 0;
    double milliseconds = 0.0;
};

struct LoadResult {
    bool ok = false;
    std::string error;
    int version = 0;
    // Ids de nos referenciados que nao existem no arquivo: sinal de projeto
    // corrompido ou de versao incompativel.
    int missingReferences = 0;
    double milliseconds = 0.0;
};

class ProjectSerializer {
public:
    static constexpr const char* kExtension = "lumina";
    static constexpr const char* kFilter = "Projetos Lumina (*.lumina)";

    [[nodiscard]] static SaveResult save(const Project& project, const std::string& path);
    [[nodiscard]] static LoadResult load(const std::string& path, Project& project);

    // Variantes em memoria, usadas pelo autosave e pelo clipboard interno.
    [[nodiscard]] static std::string toJson(const Project& project);
    [[nodiscard]] static bool fromJson(const std::string& json, Project& project,
                                       std::string* error);

    // Filtro de arquivos automatico para o dialogo abrir/salvar.
    [[nodiscard]] static QString fileDialogFilter();

    // Pasta onde ficam os autosaves: ao lado do projeto, em .lumina/autosave.
    [[nodiscard]] static QString autosavePathFor(const QString& projectPath);
    [[nodiscard]] static QString exportDirectoryFor(const QString& projectPath);
    // Onde moram os proxies: pasta do projeto + /proxies
    [[nodiscard]] static QString proxyDirectoryFor(const QString& projectPath);
};

}  // namespace lmn::io
