#include "TestSupport.h"

#include <QGuiApplication>
#include <QtGlobal>

// O runner precisa de um QGuiApplication porque alguns testes pedem um contexto
// OpenGL. QApplication seria desatualizado aqui (e exige display), entao a
// aplicacao e criada como QGuiApplication, que funciona tanto com quanto sem
// servidor X.
int main(int argc, char** argv) {
    // Plataforma minima: em CI sem display, evita tentar abrir uma janela.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("lumina-tests"));

    return lmn::test::runAll(argc, argv);
}
