// ImageCache.h - Cache de imagens carregadas do disco.
//
// Um projeto com 50 fotos nao deve reler 50 arquivos a cada frame. O cache e
// compartilhado entre a UI (miniaturas) e a camada de midia (texturas), e
// respeita um orcamento de memoria para nao estourar em maquinas modestas.
#pragma once

#include <map>
#include <memory>

#include <QImage>
#include <QString>

namespace lmn::nodes {

class ImageCache {
public:
    static ImageCache& instance();

    // Carrega a imagem em RGBA8888. Devolve nullptr se nao conseguir ler.
    // O ponteiro fica valido ate clear() ou evict() daquela imagem.
    [[nodiscard]] QImage* get(const QString& path);

    // Versao com tamanho maximo: para miniaturas, nao vale a pena decodificar
    // um TIFF de 8000px inteiro.
    [[nodiscard]] QImage* getScaled(const QString& path, int maxDimension);

    void evict(const QString& path);
    void clear();

    // Orcamento em bytes. Acima disso, as imagens menos recentes saem.
    void setMemoryBudget(size_t bytes);
    [[nodiscard]] size_t memoryBudget() const { return m_budget; }
    [[nodiscard]] size_t usedBytes() const { return m_used; }
    [[nodiscard]] size_t count() const { return m_images.size(); }

private:
    void evictLocked(size_t targetBytes);

    struct Entry {
        std::unique_ptr<QImage> image;
        size_t bytes = 0;
        quint64 lastUse = 0;
    };

    std::map<QString, Entry> m_images;
    size_t m_budget = 512ull * 1024 * 1024;   // 512 MB
    size_t m_used = 0;
    quint64 m_clock = 0;
};

}  // namespace lmn::nodes
