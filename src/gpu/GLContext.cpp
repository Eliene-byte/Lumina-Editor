#include "GLContext.h"

#include <QMap>
#include <QMutex>
#include <QOpenGLShader>
#include <QStringList>

#include <QtGlobal>

namespace lmn::gpu {
namespace {

QMutex& logMutex() {
    static QMutex m;
    return m;
}

QMap<QString, QString>& logMap() {
    static QMap<QString, QString> map;
    return map;
}

}  // namespace

QString GLContext::rendererString() const {
    return QString::fromLatin1(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
}

bool GLContext::initialize() {
    if (m_initialized) return true;

    // initializeOpenGLFunctions() carrega os ponteiros de funcao do contexto
    // corrente. Sem contexto valido ele falha e nao ha o que fazer.
    if (!initializeOpenGLFunctions()) return false;

    const auto str = [](GLenum e) {
        const auto* s = glGetString(e);
        return s ? std::string(reinterpret_cast<const char*>(s)) : std::string();
    };

    m_caps.glVersion = str(GL_VERSION);
    m_caps.glslVersion = str(GL_SHADING_LANGUAGE_VERSION);
    m_caps.vendor = str(GL_VENDOR);
    m_caps.renderer = str(GL_RENDERER);

    GLint value = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
    m_caps.maxTextureSize = value > 0 ? value : 2048;
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &value);
    m_caps.maxTextureUnits = value > 0 ? value : 16;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &value);
    m_caps.maxColorAttachments = value > 0 ? value : 8;

    // Extensoes de ponto flutuante: sem elas caimos para RGBA8 e o
    // composicionamento em linear fica limitado em videos de 8 bits.
    const std::string ext = m_caps.glVersion;  // em core profile nao ha lista
    Q_UNUSED(ext);

    const auto hasExt = [](const char* name) {
        GLint count = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);
        for (GLint i = 0; i < count; ++i) {
            const auto* e = glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i));
            if (e && qstrcmp(reinterpret_cast<const char*>(e), name) == 0) return true;
        }
        return false;
    };

    m_caps.hasHalfFloat =
        hasExt("GL_EXT_color_buffer_half_float") || hasExt("GL_EXT_color_buffer_float");
    m_caps.hasFloat = hasExt("GL_EXT_color_buffer_float");

    // Detecta rasterizadores por software. Neles a GPU nao ajuda em nada e o
    // editor precisa avisar e sugerir o modo leve.
    const std::string r = m_caps.renderer;
    const char* software[] = {"llvmpipe", "softpipe", "Microsoft Basic Render",
                              "SwiftShader", "Software Rasterizer"};
    m_caps.isSoftware = false;
    for (const char* needle : software) {
        if (r.find(needle) != std::string::npos) {
            m_caps.isSoftware = true;
            break;
        }
    }
    m_caps.isHardware = !m_caps.isSoftware;

    m_initialized = true;
    return true;
}

bool GLContext::checkError(const char* where) const {
    const GLenum err = glGetError();
    if (err == GL_NO_ERROR) return true;

    const char* name = "GL_NO_ERROR";
    switch (err) {
        case GL_INVALID_ENUM:                  name = "GL_INVALID_ENUM"; break;
        case GL_INVALID_VALUE:                 name = "GL_INVALID_VALUE"; break;
        case GL_INVALID_OPERATION:             name = "GL_INVALID_OPERATION"; break;
        case GL_INVALID_FRAMEBUFFER_OPERATION: name = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
        case GL_OUT_OF_MEMORY:                 name = "GL_OUT_OF_MEMORY"; break;
        default: break;
    }
    qWarning("[lumina][gl] erro em %s: %s (0x%x)", where ? where : "?", name, err);
    return false;
}

GLContext::Quality GLContext::detectQuality() {
    // Chamado antes de ter um contexto, so com informacao do sistema. A UI
    // refina depois que o contexto GL existe.
    return Quality::Medium;
}

// ---------------------------------------------------------------------------
// ShaderLog
// ---------------------------------------------------------------------------

void ShaderLog::setError(const std::string& name, const std::string& log) {
    QMutexLocker lock(&logMutex());
    logMap().insert(QString::fromStdString(name), QString::fromStdString(log));
}

void ShaderLog::clear(const std::string& name) {
    QMutexLocker lock(&logMutex());
    logMap().remove(QString::fromStdString(name));
}

std::string ShaderLog::get(const std::string& name) {
    QMutexLocker lock(&logMutex());
    return logMap().value(QString::fromStdString(name)).toStdString();
}

bool ShaderLog::hasErrors() {
    QMutexLocker lock(&logMutex());
    return !logMap().isEmpty();
}

std::string ShaderLog::allErrors() {
    QMutexLocker lock(&logMutex());
    QStringList parts;
    for (auto it = logMap().constBegin(); it != logMap().constEnd(); ++it) {
        parts << it.key() + ": " + it.value();
    }
    return parts.join("\n").toStdString();
}

}  // namespace lmn::gpu
