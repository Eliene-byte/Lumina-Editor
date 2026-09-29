// MediaBackend.cpp - Selecao do backend e o stub sem FFmpeg.
#include "MediaBackend.h"

#include <QFileInfo>
#include <QString>

#include <algorithm>

namespace lmn::media {
namespace {

// Quando o projeto e compilado sem FFmpeg (ou o FFmpeg nao foi encontrado no
// link), este stub responde de forma honesta: o editor abre, o grafo de nos
// funciona, e so a midia externa fica indisponivel.
class NullBackend final : public MediaBackend {
public:
    [[nodiscard]] const char* name() const override { return "stub"; }

    [[nodiscard]] DecodeCapabilities capabilities() const override {
        DecodeCapabilities c;
        c.available = false;
        c.buildInfo = "FFmpeg nao foi compilado nesta build (LUMINA_ENABLE_FFMPEG=OFF)";
        return c;
    }

    MediaProbe probeFile(const std::string& path) const override {
        MediaProbe p;
        p.ok = false;
        p.error = "decodificacao indisponivel: o FFmpeg nao faz parte desta build";
        return p;
    }

    std::shared_ptr<VideoSourcePool> createSourcePool() override { return nullptr; }
    std::unique_ptr<Encoder> createEncoder() override { return nullptr; }

    bool generateProxy(const MediaItem&, const std::string&, double,
                       std::string* error) override {
        if (error) *error = "FFmpeg nao faz parte desta build";
        return false;
    }

    void setDecodeThreads(int threads) override { m_threads = threads; }
    [[nodiscard]] int decodeThreads() const override { return m_threads; }

private:
    int m_threads = 0;
};

MediaBackend* g_backend = nullptr;

}  // namespace

MediaBackend& backend() {
    if (g_backend) return *g_backend;
#ifdef LUMINA_HAVE_FFMPEG
    // Definido em FfmpegBackend.cpp quando o FFmpeg esta disponivel.
    return ffmpegBackend();
#else
    static NullBackend stub;
    return stub;
#endif
}

void setBackend(MediaBackend* backend) {
    g_backend = backend;
}

bool shouldUseProxy(const MediaItem& item, bool globalProxyEnabled) {
    if (!item.hasProxy()) return false;
    if (!globalProxyEnabled) return false;
    // Audio e imagem nao ganham proxy: nao há ganho em trocar.
    if (item.type != MediaType::Video) return false;
    return item.useProxy;
}

}  // namespace lmn::media
