// main.cpp - Ponto de entrada.
//
//:main.cpp mantem o maximo de logica fora: o que existe aqui e o que so faz
// sentido antes e depois de existir uma janela.
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QSurfaceFormat>

#include <cstdio>

#include "HeadlessRenderer.h"
#include "HeadlessRenderer.h"
#include "expr/Expression.h"
#include "media/MediaBackend.h"
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    // A opcao --render e lida do argv antes de criar qualquer aplicacao Qt.
    // Sem isso, o modo headless dependeria de um servidor X11 existir.
    for (int i = 1; i < argc - 1; ++i) {
        const std::string arg = argv[i];
        if (arg == "-r" || arg == "--render") {
            int quality = 1;
            for (int j = 1; j < argc - 1; ++j) {
                const std::string opt = argv[j];
                if ((opt == "-q" || opt == "--qualidade") && j + 1 < argc) {
                    quality = std::atoi(argv[j + 1]);
                }
            }
            return lmn::renderHeadless(argv[i + 1], quality, false);
        }
    }

    QApplication app(argc, argv);

    QCoreApplication::setApplicationName(QStringLiteral("Lumina"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QCoreApplication::setOrganizationName(QStringLiteral("Lumina"));

    // OpenGL 3.3 core e pedido antes de qualquer widget: o Visualizador herda
    // de QOpenGLWidget, e o formato precisa estar definido antes de a janela
    // existir. Sem isso o Qt cai para 2.0 e nenhum shader compila.
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(0);
    format.setStencilBufferSize(0);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    // Sem sRGB automatico: a conversao linear para display acontece no no
    // color.viewTransform, e uma conversao do driver aqui jogaria a cor duas
    // vezes.
    format.setColorSpace(QColorSpace::SRgb);
    QSurfaceFormat::setDefaultFormat(format);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Lumina - editor de video com composicao por nos"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption proxyOption(
        QStringList{QStringLiteral("p"), QStringLiteral("modo-leve")},
        QStringLiteral("Abre em modo leve: previa reduzida e cache pequeno."));
    parser.addOption(proxyOption);

    QCommandLineOption qualityOption(
        QStringList{QStringLiteral("q"), QStringLiteral("qualidade")},
        QStringLiteral("Qualidade inicial: 0 (leve), 1 (normal) ou 2 (alta)."),
        QStringLiteral("nivel"));
    parser.addOption(qualityOption);

    QCommandLineOption renderOption(
        QStringList{QStringLiteral("r"), QStringLiteral("render")},
        QStringLiteral("Renderiza um quadro e sai. Sem janela."),
        QStringLiteral("arquivo"));
    parser.addOption(renderOption);

    QCommandLineOption compOption(
        QStringLiteral("composicao"),
        QStringLiteral("Composicao a renderizar (id). Padrao: a primeira."),
        QStringLiteral("id"));
    parser.addOption(compOption);

    parser.process(app);

    // O motor de expressoes e global; instalado aqui para que qualquer Property
    // possa ter expressao, mesmo antes de existir uma janela.
    lmn::expr::installAsCoreBackend();

    // Modo sem interface: renderizar um quadro e sair. E o que permite usar o
    // editor em um servidor ou em um pipeline de CI.
    if (parser.isSet(renderOption)) {
        const QString outPath = parser.value(renderOption);
        if (outPath.isEmpty()) {
            std::fprintf(stderr, "erro: --render exige um caminho de saida\n");
            return 2;
        }

        // A renderizacao em modo headless precisa de QGuiApplication (nao
        // QApplication): criar widgets sem display falha em Linux.
        return lmn::renderHeadless(outPath.toStdString(),
                                   parser.value(compOption, QStringLiteral("1")).toInt(),
                                   parser.isSet(proxyOption));
    }

    lmn::ui::MainWindow window;
    window.bootstrap();

    if (parser.isSet(proxyOption)) window.setLightweightMode(true);
    if (parser.isSet(qualityOption)) {
        const int level = parser.value(qualityOption).toInt();
        if (level >= 0 && level <= 2) window.setQuality(level);
    }

    window.show();

    // Arquivo na linha de comando.
    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty()) {
        const QString path = positional.first();
        if (QFileInfo::exists(path)) {
            window.openProject(path);
        } else {
            std::fprintf(stderr, "aviso: arquivo nao encontrado: %s\n",
                         path.toUtf8().constData());
        }
    }

    return app.exec();
}
