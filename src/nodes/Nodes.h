// Nodes.h - Entrada da biblioteca de nos.
#pragma once

namespace lmn::nodes {

// Registra todos os tipos. Chamar uma unica vez na inicializacao, antes de
// qualquer Project ser criado.
void registerAllNodes();

// Registros individuais (usados em testes para montar um subconjunto).
void registerSourceNodes();
void registerColorNodes();
void registerFilterNodes();
void registerMergeNodes();
void registerTimeNodes();
void registerMediaNodes();

[[nodiscard]] int registeredNodeTypeCount();

}  // namespace lmn::nodes
