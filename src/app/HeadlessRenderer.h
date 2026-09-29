// HeadlessRenderer.h - Renderizacao sem janela.
//
// Usado por --render e pelos testes de integracao. Existe separado da
// aplicacao para que o caminho sem interface possa ser testado sem subir uma
// janela.
#pragma once

#include <string>

namespace lmn {

// Renderiza um quadro de um projeto novo e grava em PNG. Devolve 0 em caso de
// sucesso, 1 em falha de uso, 2 em falha de render.
int renderHeadless(const std::string& outputPath, int compId, bool lightweight);

}  // namespace lmn
