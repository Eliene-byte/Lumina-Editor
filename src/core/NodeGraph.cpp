#include "NodeGraph.h"

#include <algorithm>
#include <queue>

#include "NodeRegistry.h"

namespace lmn {

Node* NodeGraph::createNode(const std::string& type, const std::string& name) {
    // Ids sao monotonicos e nunca reutilizados dentro de um grafo: isso evita
    // que desfazer fique com referencias penduradas.
    NodeId id = 1;
    while (m_nodes.count(id)) ++id;
    Node node = NodeRegistry::instance().create(type, id, name);
    return addNode(std::move(node));
}

Node* NodeGraph::addNode(Node node) {
    const NodeId id = node.id();
    if (id == kInvalidId || m_nodes.count(id)) return nullptr;
    Node& stored = m_nodes.emplace(id, std::move(node)).first->second;
    touch();
    return &stored;
}

bool NodeGraph::removeNode(NodeId id) {
    const auto it = m_nodes.find(id);
    if (it == m_nodes.end()) return false;

    for (auto& [nid, n] : m_nodes) {
        if (nid == id) continue;
        for (int s = 0; s < n.inputCount(); ++s) {
            if (n.input(s) == id) n.clearInput(s);
        }
        if (n.parent() == id) n.setParent(kInvalidId);
    }
    if (m_output == id) m_output = kInvalidId;

    m_nodes.erase(it);
    touch();
    return true;
}

void NodeGraph::clearNode(NodeId id) {
    Node* n = node(id);
    if (n) n->clearAllInputs();
    touch();
}

Node* NodeGraph::node(NodeId id) {
    const auto it = m_nodes.find(id);
    return it == m_nodes.end() ? nullptr : &it->second;
}

const Node* NodeGraph::node(NodeId id) const {
    const auto it = m_nodes.find(id);
    return it == m_nodes.end() ? nullptr : &it->second;
}

std::vector<NodeId> NodeGraph::nodeIds() const {
    std::vector<NodeId> out;
    out.reserve(m_nodes.size());
    for (const auto& [id, n] : m_nodes) out.push_back(id);
    return out;
}

std::vector<Node> NodeGraph::nodes() const {
    std::vector<Node> out;
    out.reserve(m_nodes.size());
    for (const auto& [id, n] : m_nodes) out.push_back(n);
    return out;
}

bool NodeGraph::connect(NodeId from, int fromOutput, NodeId to, int toInput) {
    if (from == to || !contains(from) || !contains(to)) return false;

    Node* dst = node(to);
    if (toInput < 0) return false;

    // Impede criar ciclo: adicionar from->to so e seguro se 'from' nao for
    // alcancavel a partir de 'to' (ancestors inclui o proprio no).
    for (NodeId anc : ancestors(to)) {
        if (anc == from) return false;
    }

    dst->setInput(toInput, from, fromOutput);
    touch();
    return true;
}

bool NodeGraph::disconnect(NodeId to, int toInput) {
    Node* dst = node(to);
    if (!dst || !dst->isConnected(toInput)) return false;
    dst->clearInput(toInput);
    touch();
    return true;
}

void NodeGraph::disconnectAll(NodeId nodeId) {
    Node* n = node(nodeId);
    if (!n) return;
    n->clearAllInputs();
    for (auto& [id, other] : m_nodes) {
        for (int s = 0; s < other.inputCount(); ++s) {
            if (other.input(s) == nodeId) other.clearInput(s);
        }
    }
    touch();
}

std::vector<Connection> NodeGraph::connections() const {
    std::vector<Connection> out;
    for (const auto& [id, n] : m_nodes) {
        for (int s = 0; s < n.inputCount(); ++s) {
            const NodeId from = n.input(s);
            if (from == kInvalidId) continue;
            out.push_back(Connection{from, n.inputOutputIndex(s), id, s});
        }
    }
    std::sort(out.begin(), out.end(), [](const Connection& a, const Connection& b) {
        if (a.to != b.to) return a.to < b.to;
        return a.toInput < b.toInput;
    });
    return out;
}

int NodeGraph::connectionCount() const {
    int n = 0;
    for (const auto& [id, node] : m_nodes) n += node.inputCount();
    return n;
}

void NodeGraph::walk(NodeId id, std::vector<NodeId>& out, SeenSet& seen) const {
    if (id == kInvalidId) return;
    if (id >= seen.size()) seen.resize(id + 1, 0);
    if (seen[id]) return;
    const Node* n = node(id);
    if (!n) return;
    seen[id] = 1;
    for (int s = 0; s < n->inputCount(); ++s) walk(n->input(s), out, seen);
    out.push_back(id);
}

std::vector<NodeId> NodeGraph::topologicalOrder() const {
    std::vector<NodeId> out;
    out.reserve(m_nodes.size());
    NodeId maxId = 0;
    for (const auto& [id, n] : m_nodes) maxId = std::max(maxId, id);
    SeenSet seen(static_cast<size_t>(maxId) + 1, 0);
    for (const auto& [id, n] : m_nodes) walk(id, out, seen);
    return out.size() == m_nodes.size() ? out : std::vector<NodeId>{};
}

bool NodeGraph::isAcyclic() const {
    return !topologicalOrder().empty() || m_nodes.empty();
}

int NodeGraph::depthOf(NodeId id) const {
    const Node* n = node(id);
    if (!n) return -1;
    int depth = 0;
    for (int s = 0; s < n->inputCount(); ++s) {
        const NodeId in = n->input(s);
        if (in == kInvalidId) continue;
        depth = std::max(depth, depthOf(in) + 1);
    }
    return depth;
}

std::vector<std::vector<NodeId>> NodeGraph::layers() const {
    std::map<NodeId, int> depth;
    for (const auto& [id, n] : m_nodes) depth[id] = depthOf(id);

    int maxDepth = 0;
    for (const auto& [id, d] : depth) maxDepth = std::max(maxDepth, d);

    std::vector<std::vector<NodeId>> out(static_cast<size_t>(maxDepth) + 1);
    for (const auto& [id, d] : depth) {
        out[static_cast<size_t>(d)].push_back(id);
    }
    return out;
}

std::vector<NodeId> NodeGraph::ancestors(NodeId id) const {
    std::vector<NodeId> out;
    NodeId maxId = std::max(id, 0);
    for (const auto& [nid, n] : m_nodes) maxId = std::max(maxId, nid);
    SeenSet seen(static_cast<size_t>(maxId) + 1, 0);
    walk(id, out, seen);
    return out;
}

std::vector<NodeId> NodeGraph::descendants(NodeId id) const {
    std::vector<NodeId> out;
    if (!contains(id)) return out;

    NodeId maxId = id;
    for (const auto& [nid, n] : m_nodes) maxId = std::max(maxId, nid);
    SeenSet seen(static_cast<size_t>(maxId) + 1, 0);
    seen[id] = 1;
    out.push_back(id);

    for (size_t i = 0; i < out.size(); ++i) {
        const NodeId current = out[i];
        for (const auto& [nid, n] : m_nodes) {
            if (nid >= seen.size()) seen.resize(nid + 1, 0);
            if (seen[nid]) continue;
            for (int s = 0; s < n.inputCount(); ++s) {
                if (n.input(s) == current) {
                    seen[nid] = 1;
                    out.push_back(nid);
                    break;
                }
            }
        }
    }
    return out;
}

bool NodeGraph::validate(std::string* error) const {
    const auto fail = [error](std::string msg) {
        if (error) *error = std::move(msg);
        return false;
    };

    for (const auto& [id, n] : m_nodes) {
        for (int s = 0; s < n.inputCount(); ++s) {
            const NodeId in = n.input(s);
            if (in == kInvalidId) continue;
            if (!contains(in)) {
                return fail("no " + std::to_string(id) + " entrada " + std::to_string(s) +
                            " aponta para um no inexistente");
            }
            if (in == id) return fail("no " + std::to_string(id) + " esta ligado a si mesmo");
        }
    }

    if (m_output != kInvalidId && !contains(m_output)) {
        return fail("a saida do grafo aponta para um no inexistente");
    }
    if (!topologicalOrder().empty() || m_nodes.empty()) return true;
    return fail("o grafo contem um ciclo");
}

}  // namespace lmn
