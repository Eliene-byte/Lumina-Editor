// JsonHelpers.h - Conversao entre o modelo de dados e JSON.
//
// Separado do ProjectSerializer para que o clipboard interno e o autosave
// usem exatamente o mesmo codigo de conversao - assim nao existe um segundo
// lugar onde um valor possa ser salvo de um jeito e lido de outro.
#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <memory>
#include <string>

#include "core/Node.h"
#include "core/Property.h"
#include "core/Value.h"

namespace lmn::io {

QJsonValue valueToJson(const Value& v);
// declaredType guia a leitura de arrays (2, 3 ou 4 elementos) e de strings.
Value valueFromJson(const QJsonValue& j, ValueType declaredType);

QJsonObject keyframeToJson(const Keyframe& k);
Keyframe keyframeFromJson(const QJsonObject& o);

QJsonObject propertyToJson(const Property& p);
Property propertyFromJson(const std::string& name, const QJsonObject& o,
                          ValueType declaredType);

QJsonObject nodeToJson(const Node& n);
void nodeFromJson(const QJsonObject& o, Node& node);

}  // namespace lmn::io
