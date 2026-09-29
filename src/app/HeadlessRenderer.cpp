#include "HeadlessRenderer.h"

#include <QGuiApplication>
#include <QString>
#include <QSurfaceFormat>

#include <cstdio>

#include "core/Project.h"
#include "expr/Expression.h"
#include "media/FrameExporter.h"
#include "nodes/Nodes.h"

namespace lmn {

int renderHeadless(const std::string& outputPath, int compId, bool lightweight) {
    if (outputPath.empty()) {
        std::fprintf(stderr, "erro: caminho de saida vazio\n");
        return 1;
    }

    // Formato 3.3 core: sem ele, nenhum shader compila.
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(0);
    format.setStencilBufferSize(0);
    QSurfaceFormat::setDefaultFormat(format);

    // QGuiApplication, nao QApplication: sem widgets, sem display. Em um
    // servidor Linux sem X11, o QApplication nem inicia.
    int argc = 0;
    QGuiApplication app(argc, nullptr);

    expr::installAsCoreBackend();
    nodes::registerAllNodes();

    Project project;
    project.setSize(Size{1920, 1080});
    project.setFps(30.0);
    project.createDefaultComposition();

    const CompId id = static_cast<CompId>(compId > 0 ? compId
                                                     : project.compositions().begin()->first);
    if (!project.composition(id)) {
        std::fprintf(stderr, "erro: composicao %d nao existe\n", compId);
        return 1;
    }

    // Em modo leve a previa e menor, mas para render final isso seria errado:
    // a saida tem que ter o tamanho do projeto.
    const auto result = media::FrameExporter::exportFrame(project, id, 0.0, outputPath);
    if (!result.ok) {
        std::fprintf(stderr, "erro ao renderizar: %s\n", result.error.c_str());
        return 2;
    }

    std::printf("ok: %s (%zu bytes, %.0f ms)\n", outputPath.c_str(), result.bytes,
                result.milliseconds);
    return 0;
}

}  // namespace lmn
