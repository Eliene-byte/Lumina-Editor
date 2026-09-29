#include "ShaderProgram.h"

#include <QFile>
#include <QMutex>
#include <QMutexLocker>

#include "core/Types.h"

namespace lmn::gpu {
namespace {

QMutex& cacheMutex() {
    static QMutex m;
    return m;
}

std::map<std::pair<std::string, std::string>, GLuint>& programCache() {
    static std::map<std::pair<std::string, std::string>, GLuint> cache;
    return cache;
}

size_t g_compileCount = 0;
QString g_defaultVertex;

// Fonte do vertice padrao, compartilhada entre a classe e o cache.
const QString& defaultVertexSourceImpl() {
    if (g_defaultVertex.isEmpty()) {
        g_defaultVertex = QStringLiteral(
            "#version 330 core\n"
            "// Triangulo de tela cheia: um unico VAO, sem buffers.\n"
            "out vec2 v_uv;\n"
            "void main() {\n"
            "    // Vertices do triangulo em coordenadas de clip space.\n"
            "    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
            "    v_uv = p;\n"
            "    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
            "}\n");
    }
    return g_defaultVertex;
}

QString readResource(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(f.readAll());
}

}  // namespace

ShaderProgram::~ShaderProgram() {
    if (m_id) glDeleteProgram(m_id);
}

bool ShaderProgram::compile(const std::string& key, const QString& vertexSource,
                            const QString& fragmentSource) {
    m_key = key;

    const auto cacheKey = std::make_pair(vertexSource.toStdString(),
                                         fragmentSource.toStdString());
    {
        QMutexLocker lock(&cacheMutex());
        const auto it = programCache().find(cacheKey);
        if (it != programCache().end()) {
            m_id = it->second;
            m_uniforms.clear();
            ShaderLog::clear(key);
            return true;
        }
    }

    QOpenGLShader vert(QOpenGLShader::Vertex);
    QOpenGLShader frag(QOpenGLShader::Fragment);

    if (!vert.compileSourceCode(vertexSource)) {
        ShaderLog::setError(key, vert.log().toStdString());
        return false;
    }
    if (!frag.compileSourceCode(fragmentSource)) {
        ShaderLog::setError(key, frag.log().toStdString());
        return false;
    }

    const GLuint id = glCreateProgram();
    if (!id) {
        ShaderLog::setError(key, "glCreateProgram devolveu 0");
        return false;
    }
    glAttachShader(id, vert.shaderId());
    glAttachShader(id, frag.shaderId());
    glLinkProgram(id);

    GLint linked = GL_FALSE;
    glGetProgramiv(id, GL_LINK_STATUS, &linked);
    // O objeto shader e destruido aqui; o programa guarda sua propria copia.
    vert.destroy();
    frag.destroy();

    if (linked != GL_TRUE) {
        GLint len = 0;
        glGetProgramiv(id, GL_INFO_LOG_LENGTH, &len);
        std::string log(len > 0 ? static_cast<size_t>(len) : 1, '\0');
        if (len > 0) glGetProgramInfoLog(id, len, nullptr, log.data());
        ShaderLog::setError(key, log);
        glDeleteProgram(id);
        return false;
    }

    m_id = id;
    m_uniforms.clear();
    {
        QMutexLocker lock(&cacheMutex());
        programCache()[cacheKey] = id;
        ++g_compileCount;
    }
    ShaderLog::clear(key);
    return true;
}

bool ShaderProgram::compileResource(const std::string& key, const QString& fragmentPath) {
    const QString frag = readResource(fragmentPath);
    if (frag.isEmpty()) {
        ShaderLog::setError(key, "shader nao encontrado: " + fragmentPath.toStdString());
        return false;
    }
    return compile(key, defaultVertexSourceImpl(), frag);
}

QString ShaderProgram::defaultVertexSource() {
    return defaultVertexSourceImpl();
}

bool ShaderProgram::compileFragment(const std::string& key, const QString& fragmentSource) {
    return compile(key, defaultVertexSourceImpl(), fragmentSource);
}

int ShaderProgram::location(const char* name) {
    const auto it = m_uniforms.find(name);
    if (it != m_uniforms.end()) return it->second;
    const int loc = glGetUniformLocation(m_id, name);
    m_uniforms.emplace(name, loc);
    return loc;
}

void ShaderProgram::bind() const {
    glUseProgram(m_id);
}

void ShaderProgram::setFloat(const char* name, float v) {
    const int loc = location(name);
    if (loc >= 0) glUniform1f(loc, v);
}

void ShaderProgram::setInt(const char* name, int v) {
    const int loc = location(name);
    if (loc >= 0) glUniform1i(loc, v);
}

void ShaderProgram::setBool(const char* name, bool v) {
    setInt(name, v ? 1 : 0);
}

void ShaderProgram::setVec2(const char* name, const Vec2& v) {
    const int loc = location(name);
    if (loc >= 0) glUniform2f(loc, static_cast<float>(v.x), static_cast<float>(v.y));
}

void ShaderProgram::setVec3(const char* name, const Vec3& v) {
    const int loc = location(name);
    if (loc >= 0) glUniform3f(loc, static_cast<float>(v.x), static_cast<float>(v.y),
                              static_cast<float>(v.z));
}

void ShaderProgram::setVec4(const char* name, const Vec4& v) {
    const int loc = location(name);
    if (loc >= 0) {
        glUniform4f(loc, static_cast<float>(v.x), static_cast<float>(v.y),
                    static_cast<float>(v.z), static_cast<float>(v.w));
    }
}

void ShaderProgram::setColor(const char* name, const Color& c) {
    setVec4(name, c.rgba());
}

void ShaderProgram::setMat3(const char* name, const Transform& m) {
    const int loc = location(name);
    if (loc >= 0) glUniformMatrix3fv(loc, 1, GL_FALSE, m.m);
}

void ShaderProgram::setMat3T(const char* name, const Transform& m) {
    const int loc = location(name);
    if (loc >= 0) {
        // GLSL colunas: transpoe a linha-major para nao trocar x e y na tela.
        const GLfloat t[9] = {
            static_cast<GLfloat>(m.m[0]), static_cast<GLfloat>(m.m[3]),
            static_cast<GLfloat>(m.m[6]), static_cast<GLfloat>(m.m[1]),
            static_cast<GLfloat>(m.m[4]), static_cast<GLfloat>(m.m[7]),
            static_cast<GLfloat>(m.m[2]), static_cast<GLfloat>(m.m[5]),
            static_cast<GLfloat>(m.m[8]),
        };
        glUniformMatrix3fv(loc, 1, GL_FALSE, t);
    }
}

void ShaderProgram::setFloatArray(const char* name, const float* v, int count) {
    const int loc = location(name);
    if (loc >= 0) glUniform1fv(loc, count, v);
}

void ShaderProgram::bindTexture(const char* name, int unit, GLuint texture) {
    const int loc = location(name);
    if (loc < 0) return;
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(loc, unit);
}

void ShaderProgram::invalidateAll() {
    QMutexLocker lock(&cacheMutex());
    for (auto& [key, id] : programCache()) glDeleteProgram(id);
    programCache().clear();
}

size_t ShaderProgram::cachedCount() {
    QMutexLocker lock(&cacheMutex());
    return programCache().size();
}

size_t ShaderProgram::compileCount() {
    QMutexLocker lock(&cacheMutex());
    return g_compileCount;
}

}  // namespace lmn::gpu
