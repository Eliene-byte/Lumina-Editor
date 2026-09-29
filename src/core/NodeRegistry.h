// NodeRegistry.h - Catalogo de tipos de no.
//
// Cada tipo declara: nome, categoria, propriedades com valores padrao, nomes
// das entradas/saidas e o shader GLSL associado. A UI (biblioteca de nos,
// inspector, serializacao) e o compilador de grafos leem tudo daqui, entao
// adicionar um no novo e registrar um NodeTypeDesc - sem tocar no resto.
#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "Node.h"
#include "Property.h"
#include "Value.h"

namespace lmn {

struct PropertySpec {
    std::string name;
    ValueType type = ValueType::Double;
    Value defaultValue = Value(0.0);
    PropertyUi ui = PropertyUi::Default;
    double min = 0.0;
    double max = 1.0;
    bool clamps = false;
    bool animatable = true;
    std::string group;        // secao no inspector ("Transform", "Blur", ...)
    std::string tooltip;
    std::vector<std::string> options;  // para PropertyUi::Enum
    bool expressionEnabled = true;
};

struct NodeTypeDesc {
    std::string type;          // identificador unico, ex.: "fx.gaussianBlur"
    std::string displayName;   // ex.: "Gaussian Blur"
    std::string category;      // ex.: "Blur"
    std::vector<PropertySpec> properties;
    std::vector<std::string> inputNames;    // vazio = nao tem entradas
    std::vector<std::string> outputNames;   // {"Output"} por padrao
    std::string shader;        // recurso GLSL, ex.: "shaders/blur.frag"
    bool isGenerator = false;  // true = nao depende de entrada
    bool isFilter = false;     // true = consome uma entrada e devolve outra
    bool isMerger = false;     // true = combina duas entradas
    bool supports3D = false;
    bool hasTransform = true;  // cria position/scale/rotation/anchor/opacity
    // Flags que a camada de midia e a UI consultam sem conhecer o shader.
    bool hasMedia = false;         // precisa de um SourceProvider (video/imagem)
    bool hasCompositionRef = false;// aninha outra composicao
    bool isAudio = false;          // nao produz imagem
    std::string description;

    [[nodiscard]] int inputCount() const { return static_cast<int>(inputNames.size()); }
    [[nodiscard]] int outputCount() const {
        return outputNames.empty() ? 1 : static_cast<int>(outputNames.size());
    }
    [[nodiscard]] const PropertySpec* findProperty(const std::string& n) const;
};

// Cria um Node ja populado com as propriedades do spec. Tipos com logica
// propria registram uma factory que sobrescreve isto.
using NodeFactory = std::function<Node(NodeId, std::string name)>;

class NodeRegistry {
public:
    static NodeRegistry& instance();

    // Registra um tipo. Substitui o anterior se o id ja existir (util em testes).
    void registerType(const NodeTypeDesc& desc, NodeFactory factory = {});

    [[nodiscard]] const NodeTypeDesc* desc(const std::string& type) const;
    [[nodiscard]] bool has(const std::string& type) const { return desc(type) != nullptr; }

    // Instancia um no completo com id e nome. Retorna um no vazio se o tipo
    // nao existir, para o carregador nunca quebrar.
    [[nodiscard]] Node create(const std::string& type, NodeId id,
                              const std::string& name = {}) const;

    [[nodiscard]] std::vector<std::string> categories() const;
    [[nodiscard]] std::vector<const NodeTypeDesc*> typesInCategory(const std::string& cat) const;
    [[nodiscard]] std::vector<const NodeTypeDesc*> allTypes() const;
    [[nodiscard]] size_t typeCount() const { return m_types.size(); }

    // Remove um tipo. Usado por testes.
    bool unregisterType(const std::string& type);

private:
    NodeRegistry() = default;

    struct Entry {
        NodeTypeDesc desc;
        NodeFactory factory;
    };

    std::map<std::string, Entry> m_types;
};

// Aplica um PropertySpec a uma Property recem-criada.
Property propertyFromSpec(const PropertySpec& spec);

// Atalho para registrar tipos com inicializacao de lista.
#define LUMINA_REGISTER_NODE(desc) \
    ::lmn::NodeRegistry::instance().registerType(desc)

}  // namespace lmn
