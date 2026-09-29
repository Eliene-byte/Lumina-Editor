// NodeUniforms.h - Onde cada tipo de no diz como preencher seus uniforms.
//
// O Compositor nao conhece nenhum no especifico: ele pergunta a tabela e
// preenche o que o binder pedir. Assim src/nodes guarda o conhecimento de
// shader junto do tipo, e src/gpu continua generico.
#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "ShaderProgram.h"
#include "core/Node.h"
#include "core/Property.h"

namespace lmn::gpu {

using UniformBinder = std::function<void(ShaderProgram&, const Node&, Time,
                                         const ExpressionContext&)>;

class NodeUniformRegistry {
public:
    static NodeUniformRegistry& instance();

    void registerBinder(const std::string& nodeType, UniformBinder binder);
    [[nodiscard]] UniformBinder find(const std::string& nodeType) const;
    [[nodiscard]] bool has(const std::string& nodeType) const;
    void clear();

private:
    std::unordered_map<std::string, UniformBinder> m_binders;
};

// Atalhos para os binders mais comuns.
void setPropertyF(ShaderProgram& p, const Node& node, const char* uniform, Time t,
                  const ExpressionContext& ctx);
void setPropertyI(ShaderProgram& p, const Node& node, const char* uniform, Time t,
                  const ExpressionContext& ctx);
void setPropertyV2(ShaderProgram& p, const Node& node, const char* uniform, Time t,
                   const ExpressionContext& ctx);
void setPropertyV3(ShaderProgram& p, const Node& node, const char* uniform, Time t,
                   const ExpressionContext& ctx);
void setPropertyV4(ShaderProgram& p, const Node& node, const char* uniform, Time t,
                   const ExpressionContext& ctx);
void setPropertyColor(ShaderProgram& p, const Node& node, const char* uniform, Time t,
                      const ExpressionContext& ctx);
void setPropertyBool(ShaderProgram& p, const Node& node, const char* uniform, Time t,
                     const ExpressionContext& ctx);
// Le a propriedade como string e mapeia para um enum do shader.
void setPropertyEnum(ShaderProgram& p, const Node& node, const char* uniform, Time t,
                     const ExpressionContext& ctx, const char* const* options,
                     int optionCount, int fallback);

}  // namespace lmn::gpu
