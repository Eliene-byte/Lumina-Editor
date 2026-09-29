// MediaPool.h - Acervo de midia do projeto.
//
// Guardar metadados aqui (e nao no no) faz o grafo leve para abrir em maquinas
// fracas: o pool e lido de um indice e so os arquivos realmente usados sao
// decodificados.
#pragma once

#include <string>
#include <vector>

#include "Types.h"

namespace lmn {

enum class MediaType : uint8_t {
    Unknown = 0,
    Video,
    Audio,
    Image,        // ainda, PSD, EXR
    Solid,
    Composition,  // gerado internamente
    NodeGroup,    // subgrafo reutilizavel
    Lut,
    Font,
    Count,
};

const char* mediaTypeName(MediaType t) noexcept;
MediaType mediaTypeFromName(const std::string& s) noexcept;

struct MediaItem {
    MediaId id = 0;
    MediaType type = MediaType::Unknown;
    std::string path;        // caminho absoluto ou relativo ao projeto
    std::string name;
    std::string hash;        // detecta arquivos alterados fora do editor

    Time duration = 0.0;
    Size size{0, 0};
    double fps = 0.0;
    int channels = 0;
    int sampleRate = 0;
    double rotation = 0.0;
    bool hasAlpha = false;
    bool isOffline = false;  // arquivo sumiu do disco

    // Proxy: versao leve gerada para timeline em maquinas fracas.
    std::string proxyPath;
    double proxyScale = 0.25;   // 0.25 = 1/4 da resolucao
    std::string proxyCodec = "prores_proxy";
    bool useProxy = true;       // alternavel pelo botao "lightweight"

    [[nodiscard]] bool isValid() const { return type != MediaType::Unknown; }
    [[nodiscard]] bool hasProxy() const { return !proxyPath.empty(); }
    [[nodiscard]] bool isAudioOnly() const { return type == MediaType::Audio; }
    [[nodiscard]] const std::string& displayName() const { return name.empty() ? path : name; }
};

class MediaPool {
public:
    [[nodiscard]] const std::vector<MediaItem>& items() const noexcept { return m_items; }
    std::vector<MediaItem>& items() noexcept { return m_items; }
    [[nodiscard]] size_t size() const noexcept { return m_items.size(); }
    [[nodiscard]] bool empty() const noexcept { return m_items.empty(); }

    [[nodiscard]] MediaItem* find(MediaId id);
    [[nodiscard]] const MediaItem* find(MediaId id) const;
    [[nodiscard]] MediaItem* findByPath(const std::string& path);

    MediaId add(MediaItem item);
    bool remove(MediaId id);
    void clear() { m_items.clear(); m_nextId = 1; }

    // Itens que precisam de proxy mas ainda nao tem.
    [[nodiscard]] std::vector<const MediaItem*> needsProxy(double maxWidth) const;
    // Soma dos arquivos proxy ja gerados, em bytes, para estimar espaco.
    [[nodiscard]] size_t proxyFileCount() const;

    // Ativa/desativa proxies em todo o acervo (botao "modo leve").
    void setUseProxy(bool on);
    [[nodiscard]] bool useProxy() const noexcept { return m_useProxy; }
    void setProxyScale(double s);

    void removeMissing(bool* modified = nullptr);

private:
    std::vector<MediaItem> m_items;
    MediaId m_nextId = 1;
    bool m_useProxy = true;
};

}  // namespace lmn
