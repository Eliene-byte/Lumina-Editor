// ShaderProgram.h - Compilacao e cache de programas GLSL.
//
// Os shaders vivem como recursos Qt (resources/shaders), entao o executavel
// portatil nao precisa de arquivos ao lado. O cache evita recompilar o mesmo
// par vertice/fragmento a cada frame.
#pragma once

#include <map>
#include <string>
#include <unordered_map>

#include <QOpenGLShader>
#include <QString>

#include "GLContext.h"

namespace lmn::gpu {

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    // Compila a partir de um par vertice/fragmento. A chave e usada como
    // identificador no painel de diagnostico. Retorna false e registra o log
    // do compilador em ShaderLog quando falha.
    bool compile(const std::string& key, const QString& vertexSource,
                 const QString& fragmentSource);

    // Carrega ":/shaders/blur.frag" e o vertice padrao.
    bool compileResource(const std::string& key, const QString& fragmentPath);

    // Compila usando o vertice padrao (triangulo de tela cheia). E o caminho
    // de 99% dos nos: todo passe de um compositor e uma textura de tela cheia.
    bool compileFragment(const std::string& key, const QString& fragmentSource);

    // Fonte do vertice padrao, exposta para testes e ferramentas.
    [[nodiscard]] static QString defaultVertexSource();

    [[nodiscard]] bool valid() const { return m_id != 0; }
    [[nodiscard]] GLuint id() const { return m_id; }
    [[nodiscard]] const std::string& key() const { return m_key; }

    void bind() const;

    // --- Uniformes ------------------------------------------------------------
    // Local e resolvido uma vez e guardado; se o shader nao tem a uniform, o
    // setter vira no-op (comportamento desejado: o mesmo codigo serve para
    // shaders que usam ou nao usam um parametro opcional).
    void setFloat(const char* name, float v);
    void setInt(const char* name, int v);
    void setBool(const char* name, bool v);
    void setVec2(const char* name, const Vec2& v);
    void setVec3(const char* name, const Vec3& v);
    void setVec4(const char* name, const Vec4& v);
    void setColor(const char* name, const Color& c);
    void setMat3(const char* name, const Transform& m);
    void setFloatArray(const char* name, const float* v, int count);
    // Matriz 3x3 transposta: o GLSL espera column-major, nossa Transform e
    // row-major, e a transposicao e o que manda a matriz para o shader.
    void setMat3T(const char* name, const Transform& m);

    void bindTexture(const char* name, int unit, GLuint texture);

    // Invalida todos os programas (perda de contexto, troca de GPU).
    static void invalidateAll();
    [[nodiscard]] static size_t cachedCount();
    [[nodiscard]] static size_t compileCount();

private:
    [[nodiscard]] int location(const char* name);

    std::string m_key;
    GLuint m_id = 0;
    std::unordered_map<std::string, int> m_uniforms;
};

}  // namespace lmn::gpu
