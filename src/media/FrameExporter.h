// FrameExporter.h - Exportacao de quadros e de video.
//
// A exportacao usa o mesmo caminho de render do preview. A diferenca e que
// aqui nao ha recurso: o alvo e a resolucao final, o encoder recebe os pixels
// prontos e nada e desenhado na tela.
#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

#include "MediaBackend.h"
#include "core/Project.h"

namespace lmn::media {

struct ExportResult {
    bool ok = false;
    std::string error;
    size_t bytes = 0;
    int framesWritten = 0;
    double milliseconds = 0.0;
    // Preenchido no dialogo para o usuario ver o que aconteceu.
    [[nodiscard]] std::string summary() const;
};

class FrameExporter {
public:
    // Renderiza um unico quadro e grava em PNG.
    static ExportResult exportFrame(const Project& project, CompId comp, Time time,
                                    const std::string& path);

    // Renderiza um intervalo e grava uma sequence de PNGs.
    static ExportResult exportSequence(const Project& project, CompId comp,
                                       TimeRange range, const std::string& directory,
                                       const std::string& prefix,
                                       std::function<bool(int, int)> progress = {});

    // Exportacao assincrona, para a interface nao travar.
    class Job {
    public:
        Job() = default;
        ~Job();
        Job(const Job&) = delete;
        Job& operator=(const Job&) = delete;

        bool startMovie(const Project& project, CompId comp, TimeRange range,
                        const std::string& path, const Encoder::Settings& settings);
        bool startSequence(const Project& project, CompId comp, TimeRange range,
                           const std::string& directory, const std::string& prefix);

        // true enquanto o trabalho roda.
        [[nodiscard]] bool isRunning() const { return m_running; }
        void cancel() { m_cancel.store(true); }

        [[nodiscard]] int frameIndex() const { return m_frame.load(); }
        [[nodiscard]] int frameCount() const { return m_total; }
        [[nodiscard]] bool finished() const { return m_finished; }
        [[nodiscard]] const ExportResult& result() const { return m_result; }
        [[nodiscard]] std::string progressText() const;

        void wait();

    private:
        void run(const Project& project, CompId comp, TimeRange range,
                 const std::string& target, const Encoder::Settings* settings,
                 bool sequence);

        std::thread m_thread;
        std::atomic<bool> m_cancel{false};
        std::atomic<bool> m_running{false};
        std::atomic<bool> m_finished{false};
        std::atomic<int> m_frame{0};
        int m_total = 0;
        ExportResult m_result;
    };
};

}  // namespace lmn::media
