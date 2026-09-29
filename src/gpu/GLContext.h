// GLContext.h - Funcoes e capacidades OpenGL.
//
// Usamos as classes de funcoes do proprio Qt (QOpenGLFunctions_3_3_Core) em vez
// de trazer glad/GLEW: e uma dependencia a menos e a versao de GLSL que
// precisamos ja e exatamente a que o Qt cobre.
#pragma once

#include <string>

#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QString>

namespace lmn::gpu {

class GLContext : public QOpenGLFunctions_3_3_Core {
public:
    // Deve ser chamada com um contexto corrente. Idempotente.
    bool initialize();
    [[nodiscard]] bool isInitialized() const { return m_initialized; }

    struct Caps {
        std::string glVersion;
        std::string glslVersion;
        std::string vendor;
        std::string renderer;
        int maxTextureSize = 2048;
        int maxTextureUnits = 16;
        int maxColorAttachments = 8;
        bool hasHalfFloat = false;    // FBO em RGBA16F
        bool hasFloat = false;        // FBO em RGBA32F
        bool hasDebugOutput = false;
        bool isSoftware = false;      // Mesa/llvmpipe: temos que avisar o usuario
        // Em maquinas fracas isso e falso e o modo leve entra em cena.
        bool isHardware = true;
    };
    [[nodiscard]] const Caps& caps() const { return m_caps; }

    // Verifica glGetError e manda o resultado para qWarning. Barato: o
    // chamador decide se chama (so em debug e na UI).
    bool checkError(const char* where) const;

    // Sugestao de qualidade Automatico para maquinas modestas.
    enum class Quality { Low, Medium, High };
    [[nodiscard]] static Quality detectQuality();

    [[nodiscard]] QString rendererString() const;

private:
    bool m_initialized = false;
    Caps m_caps;
};

// Erros de shader coletados para o painel de diagnostico da UI.
class ShaderLog {
public:
    static void setError(const std::string& name, const std::string& log);
    static void clear(const std::string& name);
    static std::string get(const std::string& name);
    [[nodiscard]] static bool hasErrors();
    static std::string allErrors();
};

}  // namespace lmn::gpu
