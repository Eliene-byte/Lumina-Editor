#include "JsonHelpers.h"

#include <QJsonArray>

#include "core/NodeRegistry.h"

namespace lmn::io {

// ---------------------------------------------------------------------------
// Value
// ---------------------------------------------------------------------------

QJsonValue valueToJson(const Value& v) {
    switch (v.type()) {
        case ValueType::None:   return {};
        case ValueType::Bool:   return v.asBool();
        case ValueType::Double: return v.asDouble();
        case ValueType::String: return QString::fromStdString(v.asString());
        case ValueType::Color: {
            const Color c = v.asColor();
            return QJsonArray{c.r, c.g, c.b, c.a};
        }
        case ValueType::Vec2: {
            const Vec2 p = v.asVec2();
            return QJsonArray{p.x, p.y};
        }
        case ValueType::Vec3: {
            const Vec3 p = v.asVec3();
            return QJsonArray{p.x, p.y, p.z};
        }
        case ValueType::Vec4: {
            const Vec4 p = v.asVec4();
            return QJsonArray{p.x, p.y, p.z, p.w};
        }
    }
    return {};
}

Value valueFromJson(const QJsonValue& j, ValueType declaredType) {
    switch (declaredType) {
        case ValueType::None: {
            // Sem tipo declarado: infere pelo formato do JSON.
            if (j.isBool()) return Value(j.toBool());
            if (j.isDouble()) return Value(j.toDouble());
            if (j.isString()) return Value(j.toString().toStdString());
            if (j.isArray()) {
                const QJsonArray a = j.toArray();
                if (a.size() == 2) return Value(Vec2{a[0].toDouble(), a[1].toDouble()});
                if (a.size() == 3) {
                    return Value(Vec3{a[0].toDouble(), a[1].toDouble(), a[2].toDouble()});
                }
                if (a.size() == 4) {
                    return Value(Vec4{a[0].toDouble(), a[1].toDouble(),
                                      a[2].toDouble(), a[3].toDouble()});
                }
            }
            return Value();
        }
        case ValueType::Bool:   return Value(j.toBool());
        case ValueType::String: return Value(j.toString().toStdString());
        case ValueType::Double: return Value(j.toDouble());
        case ValueType::Color: {
            const QJsonArray a = j.toArray();
            return Value(Color{a.value(0).toDouble(), a.value(1).toDouble(),
                               a.value(2).toDouble(),
                               a.size() > 3 ? a[3].toDouble() : 1.0});
        }
        case ValueType::Vec2:
            return Value(Vec2{j.toArray().value(0).toDouble(), j.toArray().value(1).toDouble()});
        case ValueType::Vec3: {
            const QJsonArray a = j.toArray();
            return Value(Vec3{a.value(0).toDouble(), a.value(1).toDouble(), a.value(2).toDouble()});
        }
        case ValueType::Vec4: {
            const QJsonArray a = j.toArray();
            return Value(Vec4{a.value(0).toDouble(), a.value(1).toDouble(),
                              a.value(2).toDouble(),
                              a.size() > 3 ? a[3].toDouble() : 1.0});
        }
    }
    return Value();
}

// ---------------------------------------------------------------------------
// Keyframes
// ---------------------------------------------------------------------------

QJsonObject keyframeToJson(const Keyframe& k) {
    QJsonObject o;
    o["t"] = k.time;
    o["v"] = valueToJson(k.value);
    if (k.interp != Interp::Bezier) o["i"] = QString::fromLatin1(interpName(k.interp));
    // Guarda o ease so quando ele existe: quase toda curva e reta, e gravar
    // "0, 0" em todas as keys dobra o tamanho do arquivo sem informacao util.
    if (k.easeIn != 0.0) o["ei"] = k.easeIn;
    if (k.easeOut != 0.0) o["eo"] = k.easeOut;
    if (k.speed != 1.0) o["s"] = k.speed;
    return o;
}

Keyframe keyframeFromJson(const QJsonObject& o) {
    Keyframe k;
    k.time = o["t"].toDouble(0.0);
    k.value = valueFromJson(o["v"], ValueType::None);
    if (o.contains("i")) k.interp = interpFromName(o["i"].toString().toStdString());
    k.easeIn = o["ei"].toDouble(0.0);
    k.easeOut = o["eo"].toDouble(0.0);
    k.speed = o["s"].toDouble(1.0);
    return k;
}

// ---------------------------------------------------------------------------
// Property
// ---------------------------------------------------------------------------

QJsonObject propertyToJson(const Property& p) {
    QJsonObject o;
    o["t"] = QString::fromLatin1(valueTypeShortName(p.type()));

    // O valor so e gravado se for diferente do padrao do tipo, ou se houver
    // animacao/expressao que dependa dele.
    const bool isDefault = (p.baseValue() == defaultValueFor(p.type()));
    if (!isDefault || p.hasAnimation() || p.hasExpression()) {
        o["v"] = valueToJson(p.baseValue());
    }
    if (p.hasAnimation()) {
        QJsonArray keys;
        keys.reserve(static_cast<int>(p.keyframes().size()));
        for (const auto& k : p.keyframes()) keys.append(keyframeToJson(k));
        o["k"] = keys;
    }
    if (p.hasExpression()) {
        o["x"] = QString::fromStdString(p.expression());
        if (!p.expressionEnabled()) o["xe"] = false;
    }
    if (p.minimum() != 0.0 || p.maximum() != 1.0) {
        o["r"] = QJsonArray{p.minimum(), p.maximum()};
    }
    if (p.clampsToRange()) o["cl"] = true;
    if (!p.isAnimatable()) o["na"] = true;
    if (p.ui() != PropertyUi::Default) o["ui"] = static_cast<int>(p.ui());
    if (!p.groupIsDefault()) o["g"] = QString::fromStdString(p.group());
    if (!p.tooltip().empty()) o["tip"] = QString::fromStdString(p.tooltip());
    return o;
}

Property propertyFromJson(const std::string& name, const QJsonObject& o,
                          ValueType declaredType) {
    ValueType type = declaredType;
    if (o.contains("t")) {
        const ValueType fromFile = valueTypeFromName(o["t"].toString().toStdString());
        if (fromFile != ValueType::None) type = fromFile;
    }

    Property p;
    p.setName(name);
    p.setForceType(type);
    if (o.contains("v")) p.setBaseValue(valueFromJson(o["v"], type));

    if (const QJsonArray r = o["r"].toArray(); r.size() == 2) {
        p.setRange(r[0].toDouble(), r[1].toDouble());
    }
    if (o["cl"].toBool()) p.setClampsToRange(true);
    if (o["na"].toBool()) p.setAnimatable(false);
    if (o.contains("ui")) p.setUi(static_cast<PropertyUi>(o["ui"].toInt()));
    if (o.contains("g")) p.setGroup(o["g"].toString().toStdString());
    if (o.contains("tip")) p.setTooltip(o["tip"].toString().toStdString());

    if (const QJsonArray keys = o["k"].toArray(); !keys.isEmpty()) {
        auto& vec = p.keyframes();
        vec.clear();
        vec.reserve(static_cast<size_t>(keys.size()));
        for (const auto& k : keys) {
            Keyframe kf = keyframeFromJson(k.toObject());
            // A key precisa ter o mesmo tipo da propriedade, senao a
            // interpolacao devolve o valor padrao e a curva fica plana.
            if (type != ValueType::None && kf.value.type() != type) {
                Value converted = kf.value;
                if (converted.convertTo(type)) kf.value = converted;
            }
            vec.push_back(std::move(kf));
        }
    }
    if (o.contains("x")) p.setExpression(o["x"].toString().toStdString());
    if (o.contains("xe")) p.setExpressionEnabled(o["xe"].toBool());

    return p;
}

// ---------------------------------------------------------------------------
// Node
// ---------------------------------------------------------------------------

QJsonObject nodeToJson(const Node& n) {
    QJsonObject o;
    o["id"] = static_cast<qint64>(n.id());
    o["ty"] = QString::fromStdString(n.type());
    if (!n.name().empty()) o["nm"] = QString::fromStdString(n.name());
    if (!n.isEnabled()) o["en"] = false;
    if (n.blendMode() != BlendMode::Over) {
        o["bm"] = QString::fromLatin1(blendModeName(n.blendMode()));
    }
    if (n.cachePolicy() != CachePolicy::Never) {
        o["ca"] = QString::fromLatin1(cachePolicyName(n.cachePolicy()));
    }
    if (n.passIndex() != 0) o["ps"] = n.passIndex();
    if (n.passIndexManual()) o["psm"] = true;
    if (static_cast<uint32_t>(n.flags()) != 0) {
        o["fl"] = static_cast<qint64>(n.flags());
    }
    if (n.graphPos().x != 0.0 || n.graphPos().y != 0.0) {
        o["gp"] = QJsonArray{n.graphPos().x, n.graphPos().y};
    }
    if (n.parent() != kInvalidId) o["pa"] = static_cast<qint64>(n.parent());

    QJsonArray inputs;
    for (int s = 0; s < n.inputCount(); ++s) {
        const NodeId from = n.input(s);
        if (from == kInvalidId) continue;
        // [origem, saida da origem, entrada]
        inputs.append(QJsonArray{static_cast<qint64>(from), n.inputOutputIndex(s), s});
    }
    if (!inputs.isEmpty()) o["in"] = inputs;

    QJsonObject props;
    for (const auto& [name, prop] : n.properties()) {
        props.insert(QString::fromStdString(name), propertyToJson(prop));
    }
    if (!props.isEmpty()) o["pr"] = props;
    return o;
}

void nodeFromJson(const QJsonObject& o, Node& node) {
    if (o.contains("nm")) node.setName(o["nm"].toString().toStdString());
    if (o.contains("en")) node.setEnabled(o["en"].toBool());
    if (o.contains("bm")) {
        node.setBlendMode(blendModeFromName(o["bm"].toString().toLatin1().constData()));
    }
    if (o.contains("ca")) {
        node.setCachePolicy(cachePolicyFromName(o["ca"].toString().toStdString()));
    }
    if (o.contains("fl")) node.setFlags(static_cast<NodeFlag>(o["fl"].toInt()));
    if (const QJsonArray gp = o["gp"].toArray(); gp.size() == 2) {
        node.setGraphPos(Vec2{gp[0].toDouble(), gp[1].toDouble()});
    }
    if (o.contains("pa")) node.setParent(static_cast<NodeId>(o["pa"].toInt()));

    node.clearAllInputs();
    if (const QJsonArray inputs = o["in"].toArray()) {
        for (const auto& c : inputs) {
            const QJsonArray arr = c.toArray();
            if (arr.size() < 3) continue;
            node.setInput(arr[2].toInt(), static_cast<NodeId>(arr[0].toInt()),
                          arr[1].toInt());
        }
    }

    // Propriedades: o registro define a forma (tipo, faixa, UI, grupo) e o
    // arquivo traz o conteudo (valor, keyframes, expressao). Juntar os dois e
    // o que permite abrir um projeto antigo depois de uma versao que_added
    // propriedades novas sem perder nada.
    if (const QJsonObject props = o["pr"].toObject(); !props.isEmpty()) {
        const NodeTypeDesc* desc = NodeRegistry::instance().desc(node.type());
        for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
            const std::string name = it.key().toStdString();
            const Property loaded = propertyFromJson(name, it.value().toObject(),
                                                     ValueType::None);

            const PropertySpec* spec = desc ? desc->findProperty(name) : nullptr;
            if (spec) {
                Property merged = propertyFromSpec(*spec);
                merged.setBaseValue(loaded.baseValue());
                merged.keyframes() = loaded.keyframes();
                merged.setExpression(loaded.expression());
                merged.setExpressionEnabled(loaded.expressionEnabled());
                node.addProperty(std::move(merged));
            } else {
                // Propriedade que nao existe mais nesta versao do editor:
                // preservamos o valor para nao perder trabalho do usuario.
                node.addProperty(loaded);
            }
        }
    }

    if (o.contains("ps")) {
        node.setPassIndex(o["ps"].toInt());
        node.setPassIndexManual(o.contains("psm") && o["psm"].toBool());
    }
}

}  // namespace lmn::io
