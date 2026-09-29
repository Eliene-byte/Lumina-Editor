#include "ImageCache.h"

#include <QFileInfo>
#include <QImageReader>

#include <algorithm>

namespace lmn::nodes {

ImageCache& ImageCache::instance() {
    static ImageCache cache;
    return cache;
}

QImage* ImageCache::get(const QString& path) {
    if (path.isEmpty()) return nullptr;

    const auto it = m_images.find(path);
    if (it != m_images.end()) {
        it->second.lastUse = ++m_clock;
        return it->second.image.get();
    }

    QImageReader reader(path);
    // Nao carregar a imagem inteira quando so queremos os metadados.
    reader.setDecideFormatFromContent(true);
    QImage img = reader.read();
    if (img.isNull()) return nullptr;

    if (img.format() != QImage::Format_RGBA8888) {
        img = img.convertToFormat(QImage::Format_RGBA8888);
    }

    Entry entry;
    entry.bytes = static_cast<size_t>(img.sizeInBytes());
    entry.lastUse = ++m_clock;
    entry.image = std::make_unique<QImage>(std::move(img));
    m_used += entry.bytes;

    QImage* raw = entry.image.get();
    m_images[path] = std::move(entry);

    evictLocked(m_budget);
    return raw;
}

QImage* ImageCache::getScaled(const QString& path, int maxDimension) {
    if (path.isEmpty() || maxDimension <= 0) return nullptr;

    // Chave separada: a versao reduzida e a imagem em tamanho cheio.
    const QString key = path + QStringLiteral("@%1").arg(maxDimension);
    const auto it = m_images.find(key);
    if (it != m_images.end()) {
        it->second.lastUse = ++m_clock;
        return it->second.image.get();
    }

    QImageReader reader(path);
    reader.setScaledSize(QSize(maxDimension, maxDimension));
    reader.setScaledClip();
    QImage img = reader.read();
    if (img.isNull()) return nullptr;
    if (img.format() != QImage::Format_RGBA8888) {
        img = img.convertToFormat(QImage::Format_RGBA8888);
    }

    Entry entry;
    entry.bytes = static_cast<size_t>(img.sizeInBytes());
    entry.lastUse = ++m_clock;
    entry.image = std::make_unique<QImage>(std::move(img));

    QImage* raw = entry.image.get();
    m_images[key] = std::move(entry);
    m_used += entry.bytes;

    evictLocked(m_budget);
    return raw;
}

void ImageCache::evict(const QString& path) {
    const auto it = m_images.find(path);
    if (it == m_images.end()) return;
    m_used = m_used > it->second.bytes ? m_used - it->second.bytes : 0;
    m_images.erase(it);
}

void ImageCache::clear() {
    m_images.clear();
    m_used = 0;
    m_clock = 0;
}

void ImageCache::setMemoryBudget(size_t bytes) {
    m_budget = std::max<size_t>(bytes, 16ull * 1024 * 1024);
    evictLocked(m_budget);
}

void ImageCache::evictLocked(size_t targetBytes) {
    if (m_used <= targetBytes) return;

    // Remove as menos usadas ate caber. A entrada recem-criada nunca e a
    // candidata, porque tem o maior m_clock.
    while (m_used > targetBytes && m_images.size() > 1) {
        auto oldest = m_images.begin();
        for (auto it = m_images.begin(); it != m_images.end(); ++it) {
            if (it->second.lastUse < oldest->second.lastUse) oldest = it;
        }
        if (oldest->second.lastUse == m_clock) break;   // nada mais a remover
        m_used = m_used > oldest->second.bytes ? m_used - oldest->second.bytes : 0;
        m_images.erase(oldest);
    }
}

}  // namespace lmn::nodes
