#include "FrameExporter.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QImage>
#include <QOffscreenSurface>
#include <QString>

#include <cstdio>

#include "OffscreenRenderer.h"
#include "core/Composition.h"
#include "gpu/Compositor.h"

namespace lmn::media {
namespace {

// Nome do arquivo do quadro: sequencia de 4 digitos, como todo mundo espera.
std::string frameFileName(const std::string& directory, const std::string& prefix,
                          int index, const char* extension) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%04d", index);
    const QString path = QString::fromStdString(directory) + QLatin1Char('/') +
                         QString::fromStdString(prefix) + QLatin1Char('_') +
                         QLatin1String(buffer) + QLatin1Char('.') +
                         QLatin1String(extension);
    return path.toStdString();
}

int frameCountFor(const Composition& comp, TimeRange range) {
    return std::max(1, static_cast<int>(std::ceil(range.duration() * comp.fps())));
}

}  // namespace

std::string ExportResult::summary() const {
    if (!ok) return error;
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "%d quadros, %.1f MB, %.1f s", framesWritten,
                  static_cast<double>(bytes) / (1024.0 * 1024.0),
                  milliseconds / 1000.0);
    return std::string(buffer);
}

ExportResult FrameExporter::exportFrame(const Project& project, CompId compId, Time time,
                                        const std::string& path) {
    QElapsedTimer timer;
    timer.start();

    ExportResult result;

    const Composition* comp = project.composition(compId);
    if (!comp) {
        result.error = "composicao inexistente";
        return result;
    }

    OffscreenRenderer renderer;
    if (!renderer.initialize(&result.error)) return result;

    gpu::Compositor compositor;
    compositor.setPool(&renderer.gl());
    compositor.setPreviewScale(1.0);

    const auto pixels = renderer.renderFrame(compositor, *comp, time, comp->size().width,
                                             comp->size().height, &project);
    if (pixels.empty()) {
        result.error = "o grafo nao produziu nenhum pixel (verifique o no de saida)";
        return result;
    }

    // QImage com profundidade de 8 bits por canal e sem alpha proprio: o PNG
    // carrega o canal, mas sem metadados de cor, para nao gravar um perfil que
    // nao corresponde ao que renderizamos.
    QImage image(pixels.data(), comp->size().width, comp->size().height,
                 comp->size().width * 4, QImage::Format_RGBA8888);

    if (!image.save(QString::fromStdString(path), "PNG")) {
        result.error = "falha ao gravar " + path;
        return result;
    }

    result.ok = true;
    result.framesWritten = 1;
    result.bytes = static_cast<size_t>(QFileInfo(QString::fromStdString(path)).size());
    result.milliseconds = timer.nsecsElapsed() / 1e6;
    return result;
}

ExportResult FrameExporter::exportSequence(const Project& project, CompId compId,
                                           TimeRange range,
                                           const std::string& directory,
                                           const std::string& prefix,
                                           std::function<bool(int, int)> progress) {
    QElapsedTimer timer;
    timer.start();

    ExportResult result;

    const Composition* comp = project.composition(compId);
    if (!comp) {
        result.error = "composicao inexistente";
        return result;
    }

    OffscreenRenderer renderer;
    if (!renderer.initialize(&result.error)) return result;

    gpu::Compositor compositor;
    compositor.setPool(&renderer.gl());
    compositor.setPreviewScale(1.0);

    const int total = frameCountFor(*comp, range);
    const double dt = 1.0 / comp->fps();

    for (int i = 0; i < total; ++i) {
        const Time t = range.in + i * dt;
        const auto pixels = renderer.renderFrame(compositor, *comp, t,
                                                 comp->size().width, comp->size().height,
                                                 &project);
        if (pixels.empty()) {
            result.error = "falha ao renderizar o quadro " + std::to_string(i);
            return result;
        }

        QImage image(pixels.data(), comp->size().width, comp->size().height,
                     comp->size().width * 4, QImage::Format_RGBA8888);
        const std::string file = frameFileName(directory, prefix, i, "png");
        if (!image.save(QString::fromStdString(file), "PNG")) {
            result.error = "falha ao gravar " + file;
            return result;
        }
        result.bytes += static_cast<size_t>(QFileInfo(QString::fromStdString(file)).size());
        ++result.framesWritten;

        if (progress && !progress(i + 1, total)) {
            result.error = "cancelado pelo usuario";
            result.milliseconds = timer.nsecsElapsed() / 1e6;
            return result;
        }
    }

    result.ok = true;
    result.milliseconds = timer.nsecsElapsed() / 1e6;
    return result;
}

// ---------------------------------------------------------------------------
// Job
// ---------------------------------------------------------------------------

FrameExporter::Job::~Job() {
    wait();
}

bool FrameExporter::Job::startMovie(const Project& project, CompId comp, TimeRange range,
                                    const std::string& path,
                                    const Encoder::Settings& settings) {
    if (m_running) return false;
    m_cancel.store(false);
    m_finished.store(false);
    m_frame.store(0);
    m_result = ExportResult{};

    // O Project e copiado para a thread: o usuario pode continuar editando
    // enquanto a exportacao roda, e o grafo que ele produz nao deve mudar
    // no meio do arquivo.
    const Project snapshot = project;
    const Encoder::Settings settingsCopy = settings;
    m_total = frameCountFor(*project.composition(comp), range);

    m_thread = std::thread([this, snapshot, comp, range, path, settingsCopy] {
        run(snapshot, comp, range, path, &settingsCopy, false);
    });
    m_running.store(true);
    return true;
}

bool FrameExporter::Job::startSequence(const Project& project, CompId comp,
                                       TimeRange range, const std::string& directory,
                                       const std::string& prefix) {
    if (m_running) return false;
    m_cancel.store(false);
    m_finished.store(false);
    m_frame.store(0);
    m_result = ExportResult{};

    const Project snapshot = project;
    m_total = frameCountFor(*project.composition(comp), range);

    m_thread = std::thread([this, snapshot, comp, range, directory, prefix] {
        run(snapshot, comp, range, directory, nullptr, true);
    });
    m_running.store(true);
    return true;
}

void FrameExporter::Job::run(const Project& project, CompId compId, TimeRange range,
                             const std::string& target, const Encoder::Settings* settings,
                             bool sequence) {
    QElapsedTimer timer;
    timer.start();
    m_running.store(true);

    const Composition* comp = project.composition(compId);
    if (!comp) {
        m_result.error = "composicao inexistente";
        m_finished.store(true);
        m_running.store(false);
        return;
    }

    OffscreenRenderer renderer;
    if (!renderer.initialize(&m_result.error)) {
        m_finished.store(true);
        m_running.store(false);
        return;
    }

    gpu::Compositor compositor;
    compositor.setPool(&renderer.gl());
    compositor.setPreviewScale(1.0);

    std::unique_ptr<Encoder> encoder;
    if (!sequence) {
        encoder = backend().createEncoder();
        if (!encoder) {
            m_result.error = "nenhum encoder disponivel (FFmpeg ausente nesta build)";
            m_finished.store(true);
            m_running.store(false);
            return;
        }
        Encoder::Settings s = *settings;
        s.size = comp->size();
        s.fps = comp->fps();
        if (!encoder->begin(s, &m_result.error)) {
            m_finished.store(true);
            m_running.store(false);
            return;
        }
    }

    const int total = m_total;
    const double dt = 1.0 / comp->fps();
    const int w = comp->size().width;
    const int h = comp->size().height;

    for (int i = 0; i < total; ++i) {
        if (m_cancel.load()) {
            m_result.error = "cancelado";
            break;
        }

        const Time t = range.in + i * dt;
        const auto pixels =
            renderer.renderFrame(compositor, *comp, t, w, h, &project);
        if (pixels.empty()) {
            m_result.error = "falha ao renderizar o quadro " + std::to_string(i);
            break;
        }

        if (sequence) {
            QImage image(pixels.data(), w, h, w * 4, QImage::Format_RGBA8888);
            const std::string file = frameFileName(target, "frame", i, "png");
            if (!image.save(QString::fromStdString(file), "PNG")) {
                m_result.error = "falha ao gravar " + file;
                break;
            }
            m_result.bytes +=
                static_cast<size_t>(QFileInfo(QString::fromStdString(file)).size());
        } else {
            if (!encoder->writeFrame(pixels.data(), w, h, w * 4, t)) {
                m_result.error = "falha ao gravar o quadro " + std::to_string(i);
                break;
            }
        }

        ++m_result.framesWritten;
        m_frame.store(i + 1);
    }

    if (encoder) {
        std::string error;
        if (!encoder->finish(&error) && m_result.error.empty()) {
            m_result.error = error;
        }
        // O encoder solta o arquivo aqui; e o momento de descobrir que o disco
        // estava cheio ou que o caminho nao existia.
        m_result.bytes +=
            static_cast<size_t>(QFileInfo(QString::fromStdString(target)).size());
    }

    m_result.milliseconds = timer.nsecsElapsed() / 1e6;
    if (m_result.error.empty()) m_result.ok = true;
    m_finished.store(true);
    m_running.store(false);
}

void FrameExporter::Job::wait() {
    if (m_thread.joinable()) m_thread.join();
}

std::string FrameExporter::Job::progressText() const {
    if (m_cancel.load()) return "cancelando...";
    if (m_finished.load()) return m_result.ok ? "concluido" : "erro: " + m_result.error;
    return "quadro " + std::to_string(m_frame.load()) + " de " +
           std::to_string(m_total);
}

}  // namespace lmn::media
