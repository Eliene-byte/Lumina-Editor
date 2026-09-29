// Compositor.h - Executa o grafo de nos e produz uma textura.
//
// O compositor e a ponte entre o modelo de dados (core) e a GPU. Ele nao sabe
// nada sobre Qt: recebe uma Composition, um tempo e devolve a textura de
// saida. Isso permite usar o mesmo caminho para o preview, para a exportacao e
// para o render em segundo plano.
#pragma once

#include <functional>
#include <map>
#include <string>
#include <unordered_map>

#include "NodeUniforms.h"
#include "RenderTarget.h"
#include "ShaderProgram.h"
#include "ShaderLoader.h"
#include "core/Composition.h"
#include "core/Node.h"
#include "core/NodeRegistry.h"
#include "core/Project.h"

namespace lmn::gpu {

// Como o compositor le a fonte de um no gerador (midia, imagem, composicao).
// A camada de midia implementa isso; sem implementacao, geradores externos
// caem no fallback (cor solida) em vez de quebrar o render.
class SourceProvider {
public:
    virtual ~SourceProvider() = default;

    struct Request {
        NodeId node = kInvalidId;
        const Node* nodeRef = nullptr;
        Time time = 0.0;
        Size compositionSize{1920, 1080};
        double fps = 30.0;
        ExpressionContext expr;
    };

    // Devolve a textura pronta, ou nullptr se ainda nao estiver disponivel.
    // Quando devolve nullptr, o compositor chama onSourceUnavailable().
    virtual GLuint textureFor(const Request& request) = 0;

    // Pre-avisa: permite ao provedor comecar a decodificar antes do frame.
    virtual void prefetch(const Request& request) { (void)request; }
};

// Uma "temporada" de render: guarda os alvos ate o fim do frame e devolve ao
// pool. Sem isso, o pool nao conseguiria reciclar nada.
class RenderContext {
public:
    explicit RenderContext(RenderTargetPool& pool, int width, int height)
        : m_pool(pool), m_width(width), m_height(height) {}

    [[nodiscard]] RenderTargetPtr acquire();
    void releaseAll();

    [[nodiscard]] int width() const { return m_width; }
    [[nodiscard]] int height() const { return m_height; }
    [[nodiscard]] RenderTargetPool& pool() { return m_pool; }

private:
    RenderTargetPool& m_pool;
    int m_width;
    int m_height;
    std::vector<RenderTargetPtr> m_acquired;
};

struct RenderStats {
    int nodesEvaluated = 0;
    int drawCalls = 0;
    int texturesAllocated = 0;
    double milliseconds = 0.0;
    [[nodiscard]] bool isValid() const { return nodesEvaluated > 0; }
};

// Desenha o triangulo de tela cheia. Todos os passes de um compositor usam
// isto; nao ha geometria para upload em nenhum ponto do pipeline.
void fullscreenTriangle();

class Compositor {
public:
    Compositor();
    ~Compositor();
    Compositor(const Compositor&) = delete;
    Compositor& operator=(const Compositor&) = delete;

    void setSourceProvider(SourceProvider* provider) { m_sources = provider; }
    void setPool(RenderTargetPool* pool) { m_pool = pool; }
    [[nodiscard]] SourceProvider* sourceProvider() const { return m_sources; }

    // Configuracoes globais que valem para todos os frames.
    void setDisplayTransform(const ColorManagementSettings& cm) { m_color = cm; }
    [[nodiscard]] const ColorManagementSettings& displayTransform() const { return m_color; }

    // Qualidade global. Em maquina fraca o modo leve reduz amostras de blur,
    // desliga dither e baixa a resolucao interna do preview.
    enum class Quality { Full, Half, Quarter };
    void setQuality(Quality q) { m_quality = q; }
    [[nodiscard]] Quality quality() const { return m_quality; }
    // Escala do preview: renderizar a 50% e esticar economiza 4x o tempo de
    // fragmento, imperceptivel durante a edicao.
    void setPreviewScale(double s) { m_previewScale = s; }
    [[nodiscard]] double previewScale() const { return m_previewScale; }

    // Renderiza a composicao em 'time' e devolve a textura de saida.
    // O alvo pertence ao RenderContext e so vale ate a proxima renderizacao.
    // Retorna nullptr se o grafo estiver vazio ou invalido.
    [[nodiscard]] GLuint render(Composition& comp, Time time, RenderContext& ctx,
                                const Project* project = nullptr,
                                RenderStats* stats = nullptr);

    // Ultima mensagem de erro (shader que nao compilou, ciclo no grafo...).
    [[nodiscard]] const std::string& lastError() const { return m_lastError; }
    [[nodiscard]] bool hasError() const { return !m_lastError.empty(); }
    void clearError() { m_lastError.clear(); }

    // Metodos comidos pelo usuario: se a camera 3D de um no mudar, todo o
    // resultado precisa ser refeito.
    void invalidateCaches() { m_shaderCacheValid = false; }

    [[nodiscard]] RenderTargetPool* pool() const { return m_pool; }

private:
    [[nodiscard]] bool prepareNode(Composition& comp, const Node& node, Time time,
                                  RenderContext& ctx, const Project* project,
                                  GLuint& outputTexture, int& depth);
    void bindStandardUniforms(ShaderProgram& program, const Node& node, Time time,
                              const Composition& comp, int width, int height);
    void setInputTexture(ShaderProgram& program, const char* uniform, GLuint texture,
                         int unit);
    [[nodiscard]] GLuint resolveInput(Composition& comp, const Node& node, int slot,
                                      Time time, RenderContext& ctx,
                                      const Project* project, int depth);
    [[nodiscard]] bool shaderFor(const Node& node, ShaderProgram*& out);

    SourceProvider* m_sources = nullptr;
    RenderTargetPool* m_pool = nullptr;
    ColorManagementSettings m_color;
    Quality m_quality = Quality::Full;
    double m_previewScale = 1.0;
    std::string m_lastError;

    // Cache de resultados por no dentro de um unico frame.
    std::unordered_map<NodeId, GLuint> m_frameResults;

    std::unordered_map<std::string, std::unique_ptr<ShaderProgram>> m_shaders;
    bool m_shaderCacheValid = true;
    int m_nextUnit = 0;
};

}  // namespace lmn::gpu
