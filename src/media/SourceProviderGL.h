// SourceProviderGL.h - O SourceProvider que traduz midia em texturas GPU.
//
// Fica entre o compositor (que so conhece texturas) e a camada de midia (que
// so conhece arquivos). E aqui que o RGBA do quadro vira uma textura
// atualizada, e que o proxy entra em jogo sem o resto do editor saber.
#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "core/Composition.h"
#include "core/Node.h"
#include "core/Project.h"
#include "gpu/Compositor.h"
#include "media/MediaBackend.h"

namespace lmn::media {

class SourceProviderGL final : public gpu::SourceProvider {
public:
    SourceProviderGL(Project& project, MediaBackend& backend);
    ~SourceProviderGL() override;

    // Precisa de um contexto corrente no momento de criar e de destruir.
    void initialize();
    void shutdown();

    void setUseProxy(bool on) { m_useProxy = on; }
    [[nodiscard]] bool useProxy() const { return m_useProxy; }

    // Escala da textura de origem. 1 = resolucao do arquivo; 0.5 = metade.
    // Baixar aqui economiza memoria de video em maquinas modestas.
    void setSourceScale(double s) { m_sourceScale = std::clamp(s, 0.1, 1.0); }

    // Invalida as texturas (arquivo alterado, ou troca de contexto).
    void invalidateAll();

    [[nodiscard]] size_t textureCount() const { return m_textures.size(); }
    [[nodiscard]] size_t bytesUsed() const;

    // Chave que identifica a textura de origem. Chaveia pelo no e pelo item,
    // nao pelo tempo: o mesmo no reusa a mesma textura e o quadro novo apenas
    // sobrescreve. Incluir o tempo criaria uma textura por frame.
    [[nodiscard]] static std::string cacheKey(NodeId node, MediaId media, bool useProxy);

    GLuint textureFor(const Request& request) override;
    void prefetch(const Request& request) override;

private:
    struct Entry {
        GLuint texture = 0;
        int width = 0;
        int height = 0;
        int64_t lastFrame = -1;
        size_t bytes = 0;
        std::shared_ptr<VideoSource> source;
        MediaId media = 0;
        std::string path;
    };

    // Cria (ou atualiza) a textura de um quadro. Devolve nullptr se ainda nao
    // houver pixel disponivel.
    GLuint upload(Entry& entry, const VideoFrame& frame);
    [[nodiscard]] Entry* findEntry(const std::string& key);
    void destroyAll();

    Project& m_project;
    MediaBackend& m_backend;
    std::shared_ptr<VideoSourcePool> m_pool;
    std::map<std::string, Entry> m_textures;
    mutable std::mutex m_mutex;
    bool m_useProxy = true;
    double m_sourceScale = 1.0;
    bool m_initialized = false;
    size_t m_budget = 256ull * 1024 * 1024;
};

}  // namespace lmn::media
