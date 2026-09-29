// NodeGraph.h - Grafo dirigido aciclico de nos de composicao.
#pragma once

#include <functional>
#include <map>
#include <vector>

#include "Node.h"

namespace lmn {

struct Connection {
    NodeId from = kInvalidId;   // origem
    int fromOutput = 0;
    NodeId to = kInvalidId;     // destino
    int toInput = 0;

    friend bool operator==(const Connection& a, const Connection& b) {
        return a.from == b.from && a.fromOutput == b.fromOutput &&
               a.to == b.to && a.toInput == b.toInput;
    }
};

class NodeGraph {
public:
    // --- Nos ------------------------------------------------------------------
    [[nodiscard]] size_t size() const noexcept { return m_nodes.size(); }
    [[nodiscard]] bool empty() const noexcept { return m_nodes.empty(); }
    void clear() { m_nodes.clear(); m_output = kInvalidId; }

    // Cria um no e registra as propriedades padrao do tipo via o registro de nos.
    Node* createNode(const std::string& type, const std::string& name = {});
    // Adiciona um no ja pronto (usado pelo carregador de projetos).
    Node* addNode(Node node);
    bool removeNode(NodeId id);
    void clearNode(NodeId id);   // desconecta e apaga

    [[nodiscard]] Node* node(NodeId id);
    [[nodiscard]] const Node* node(NodeId id) const;
    [[nodiscard]] bool contains(NodeId id) const { return m_nodes.count(id) > 0; }
    [[nodiscard]] std::vector<NodeId> nodeIds() const;              // ordem de criacao
    [[nodiscard]] std::vector<Node> nodes() const;

    // --- Saida ----------------------------------------------------------------
    [[nodiscard]] NodeId output() const noexcept { return m_output; }
    void setOutput(NodeId id) { m_output = id; }

    // --- Conexoes -------------------------------------------------------------
    bool connect(NodeId from, int fromOutput, NodeId to, int toInput);
    bool disconnect(NodeId to, int toInput);
    void disconnectAll(NodeId nodeId);
    [[nodiscard]] std::vector<Connection> connections() const;
    [[nodiscard]] int connectionCount() const;

    // --- Topologia ------------------------------------------------------------
    [[nodiscard]] bool isAcyclic() const;
    // Ordem de avaliacao. Retorna vazio se houver ciclo.
    [[nodiscard]] std::vector<NodeId> topologicalOrder() const;
    // Profundidade = caminho mais longo ate um no sem entrada. Serve para
    // empilhar visualmente os nos e para ordenar passes de mesclagem.
    [[nodiscard]] int depthOf(NodeId id) const;
    [[nodiscard]] std::vector<std::vector<NodeId>> layers() const;

    // Nos que alimentam 'id' (inclui o proprio), em ordem de avaliacao.
    [[nodiscard]] std::vector<NodeId> ancestors(NodeId id) const;
    // Nos que 'id' alimenta, incluindo o proprio.
    [[nodiscard]] std::vector<NodeId> descendants(NodeId id) const;

    // Valida o grafo inteiro; devolve false e preenche 'error' se algo estiver
    // inconsistente (conexao orfa, ciclo, entrada inexistente).
    [[nodiscard]] bool validate(std::string* error = nullptr) const;

    // ----notificacoes---------------------------------------------------------
    // Disparado sempre que a topologia muda (add/remove/connect).
    [[nodiscard]] uint64_t revision() const noexcept { return m_revision; }
    using ChangeCallback = std::function<void(uint64_t revision)>;
    void setChangeCallback(ChangeCallback cb) { m_onChange = std::move(cb); }

private:
    void touch() { ++m_revision; if (m_onChange) m_onChange(m_revision); }
    void walk(NodeId id, std::vector<NodeId>& out, SeenSet& seen) const;

    // Marcadores de visita dimensionados por id. Ids sao monotonicos e podem
    // ficar maiores que size() apos remocoes, entao o vetor cresce sob demanda.
    using SeenSet = std::vector<char>;

    std::map<NodeId, Node> m_nodes;
    NodeId m_output = kInvalidId;
    uint64_t m_revision = 1;
    ChangeCallback m_onChange;
};

}  // namespace lmn
