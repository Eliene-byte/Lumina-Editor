#include "InspectorWidget.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cctype>
#include <limits>

#include "Theme.h"
#include "ValueEditors.h"
#include "core/NodeRegistry.h"
#include "expr/Expression.h"

namespace lmn::ui {
namespace {

constexpr uint32_t kNoKey = std::numeric_limits<uint32_t>::max();

// "u_mapChannel" -> "Map channel". Nomes de uniform viram rotulos legiveis
// sem um dicionario por no.
QString labelFor(const Property& prop) {
    std::string name = prop.name();
    if (name.size() > 1 && (name[0] == 'u' || name[0] == 'U') &&
        (name[1] == '_' || std::isupper(static_cast<unsigned char>(name[1])) != 0)) {
        name = name.substr(1);
    }
    if (!name.empty() && name[0] == '_') name = name.substr(1);

    QString out;
    for (size_t i = 0; i < name.size(); ++i) {
        const char c = name[i];
        if (c == '_' || c == '.') {
            out += QLatin1Char(' ');
            continue;
        }
        if (i > 0 && std::isupper(static_cast<unsigned char>(c)) != 0 &&
            std::isupper(static_cast<unsigned char>(name[i - 1])) == 0) {
            out += QLatin1Char(' ');
            out += QChar(c).toLower();
            continue;
        }
        out += QChar(c);
    }
    if (!out.isEmpty()) out[0] = out[0].toUpper();
    return out;
}

QToolButton* iconButton(const QString& text, const QString& tip) {
    auto* b = new QToolButton();
    b->setText(text);
    b->setToolTip(tip);
    b->setFixedSize(18, 18);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}

}  // namespace

InspectorWidget::InspectorWidget(Project& project, PlaybackController& playback,
                                 QWidget* parent)
    : QWidget(parent), m_project(project), m_playback(playback) {
    m_palette = Palette::dark();

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(m_scroll);

    m_content = new QWidget();
    m_content->setStyleSheet(QStringLiteral(
        "QWidget { background-color: #212429; }"
        "QGroupBox { border: 1px solid #2f333b; border-radius: 4px; margin-top: 16px;"
        " padding-top: 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; color: #8b919c; }"));
    m_layout = new QVBoxLayout(m_content);
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->setSpacing(6);
    m_scroll->setWidget(m_content);

    auto* hint = new QLabel(QStringLiteral("Selecione um no no editor de grafos"),
                            m_content);
    hint->setObjectName(QStringLiteral("hint"));
    hint->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(hint);
    m_layout->addStretch(1);
}

InspectorWidget::~InspectorWidget() = default;

void InspectorWidget::clear() {
    m_targetComp = 0;
    m_target = kInvalidId;
    clearRows();
}

void InspectorWidget::setTarget(CompId comp, NodeId id) {
    if (m_targetComp == comp && m_target == id) return;
    m_targetComp = comp;
    m_target = id;
    rebuild();
}

void InspectorWidget::setFilter(const QString& text) {
    m_filter = text.trimmed();
    rebuild();
}

void InspectorWidget::setShowAnimatedOnly(bool on) {
    m_animatedOnly = on;
    rebuild();
}

void InspectorWidget::setUndoAvailable(bool canUndo, bool canRedo) {
    m_undoAvailable = canUndo;
    m_redoAvailable = canRedo;
}

void InspectorWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
}

void InspectorWidget::clearRows() {
    hideExpressionEditor();
    m_rows.clear();
    m_expressionRow = nullptr;

    while (QLayoutItem* item = m_layout->takeAt(0)) {
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }
}

bool InspectorWidget::matchesFilter(const Property& prop) const {
    if (m_animatedOnly && !prop.hasAnimation() && !prop.hasExpression()) return false;
    if (m_filter.isEmpty()) return true;
    if (QString::fromStdString(prop.name()).contains(m_filter, Qt::CaseInsensitive)) {
        return true;
    }
    return labelFor(prop).contains(m_filter, Qt::CaseInsensitive);
}

void InspectorWidget::showNodeHeader(const Node& node) {
    auto* box = new QGroupBox(QString::fromStdString(node.name()), m_content);
    auto* form = new QFormLayout(box);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* type = new QLabel(QString::fromStdString(node.type()), box);
    type->setObjectName(QStringLiteral("hint"));
    type->setFont(monoFont(8));
    form->addRow(QStringLiteral("tipo"), type);

    form->addRow(QStringLiteral("id"), new QLabel(QString::number(node.id()), box));

    bool anyExpr = false;
    for (const auto& [pname, prop] : node.properties()) {
        if (pname.empty()) continue;
        if (prop.hasExpression() && prop.expressionEnabled()) { anyExpr = true; break; }
    }
    if (anyExpr) {
        auto* label = new QLabel(QStringLiteral("expressoes ativas"), box);
        label->setObjectName(QStringLiteral("successText"));
        form->addRow(QString(), label);
    }

    m_layout->addWidget(box);
}

std::vector<std::pair<std::string, std::vector<InspectorWidget::Row*>>>
InspectorWidget::groupRows(std::vector<Row>& rows) {
    std::vector<std::pair<std::string, std::vector<Row*>>> groups;
    std::vector<std::string> order;

    for (auto& row : rows) {
        const std::string& group = row.prop->group();
        if (std::find(order.begin(), order.end(), group) == order.end()) {
            order.push_back(group);
            groups.emplace_back(group, std::vector<Row*>{});
        }
    }
    for (auto& row : rows) {
        const std::string& group = row.prop->group();
        for (auto& [g, list] : groups) {
            if (g == group) {
                list.push_back(&row);
                break;
            }
        }
    }
    return groups;
}

void InspectorWidget::rebuild() {
    clearRows();

    Composition* comp = m_project.composition(m_targetComp);
    Node* node = comp ? comp->graph().node(m_target) : nullptr;
    if (!node) {
        auto* hint = new QLabel(QStringLiteral("Nenhum no selecionado"), m_content);
        hint->setObjectName(QStringLiteral("hint"));
        hint->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(hint);
        m_layout->addStretch(1);
        return;
    }

    showNodeHeader(*node);
    m_targetType = node->type();

    // Filtra antes de criar os widgets: filtrar depois deixaria controles
    // orfaos no layout.
    std::vector<std::unique_ptr<Row>> rows;
    for (auto& [name, prop] : node->properties()) {
        if (!matchesFilter(prop)) continue;
        auto row = std::make_unique<Row>();
        row->name = name;
        row->prop = &prop;
        rows.push_back(std::move(row));
    }

    if (rows.empty()) {
        auto* hint = new QLabel(
            m_filter.isEmpty() ? QStringLiteral("Este no nao tem propriedades")
                               : QStringLiteral("Nada corresponde ao filtro"),
            m_content);
        hint->setObjectName(QStringLiteral("hint"));
        hint->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(hint);
        m_layout->addStretch(1);
        return;
    }

    std::vector<Row*> rowPointers;
    rowPointers.reserve(rows.size());
    for (auto& row : rows) rowPointers.push_back(row.get());

    const auto groups = groupRows(rowPointers);

    for (const auto& [groupName, groupRowsList] : groups) {
        auto* box = new QGroupBox(
            groupName.empty() ? QStringLiteral("Geral")
                              : QString::fromStdString(groupName),
            m_content);
        auto* form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setHorizontalSpacing(6);

        for (Row* row : groupRowsList) buildRow(form, *row);

        m_layout->addWidget(box);
    }

    m_layout->addStretch(1);

    // As linhas passam a viver em m_rows. Os ponteiros de groupRows apontam
    // para os objetos de `rows`, que nao mudam de endereco ao mover o
    // unique_ptr, entao Row* continua valido.
    m_rows = std::move(rows);
    refreshValues();
    for (auto& row : m_rows) {
        updateKeyButton(*row);
        updateExprButton(*row);
    }
}

void InspectorWidget::buildRow(QFormLayout* form, Row& row) {
    Property& prop = *row.prop;
    const std::string propName = row.name;
    const NodeTypeDesc* desc = NodeRegistry::instance().desc(m_targetType);
    const PropertySpec* spec = desc ? desc->findProperty(propName) : nullptr;

    std::unique_ptr<ValueEditor> editor;
    QWidget* field = nullptr;

    switch (prop.type()) {
        case ValueType::Bool:
            editor = std::make_unique<BoolEditor>(form->parentWidget(),
                                                  prop.baseValue().asBool());
            field = editor->widget();
            break;

        case ValueType::Color:
            editor = std::make_unique<ColorEditor>(form->parentWidget(),
                                                  prop.baseValue().asColor());
            field = editor->widget();
            break;

        case ValueType::String: {
            const bool isEnum = spec && spec->ui == PropertyUi::Enum;
            if (isEnum) {
                auto* e = std::make_unique<EnumEditor>(form->parentWidget(),
                                                        prop.baseValue().asString(),
                                                        spec->options);
                field = e->widget();
                editor = std::move(e);
            } else {
                const bool isFile = spec && spec->ui == PropertyUi::FilePath;
                auto* e = std::make_unique<TextEditor>(form->parentWidget(),
                                                        prop.baseValue().asString(),
                                                        isFile);
                field = e->widget();
                editor = std::move(e);
            }
            break;
        }

        case ValueType::Vec2:
        case ValueType::Vec3: {
            std::unique_ptr<VectorEditor> e;
            if (prop.type() == ValueType::Vec2) {
                e = std::make_unique<VectorEditor>(form->parentWidget(),
                                                   prop.baseValue().asVec2(),
                                                   prop.minimum(), prop.maximum(), 2);
            } else {
                e = std::make_unique<VectorEditor>(form->parentWidget(),
                                                   prop.baseValue().asVec3(),
                                                   prop.minimum(), prop.maximum(), 3);
            }
            field = e->widget();
            editor = std::move(e);
            break;
        }

        case ValueType::Double:
        default: {
            if (prop.ui() == PropertyUi::Angle) {
                auto* e = std::make_unique<AngleEditor>(form->parentWidget(),
                                                        prop.baseValue().asDouble());
                field = e->widget();
                editor = std::move(e);
            } else {
                const double span = prop.maximum() - prop.minimum();
                const int decimals = span <= 2.0 ? 2 : 3;
                const double step = std::max(span / 1000.0, 0.001);
                auto* e = std::make_unique<NumberEditor>(form->parentWidget(),
                                                        prop.baseValue().asDouble(),
                                                        prop.minimum(), prop.maximum(),
                                                        decimals, step);
                field = e->widget();
                editor = std::move(e);
            }
            break;
        }
    }

    QWidget* key = iconButton(QStringLiteral("◇"),
                              QStringLiteral("Keyframe no tempo atual"));
    QWidget* expr = iconButton(QStringLiteral("fx"),
                               QStringLiteral("Alternar expressao"));

    auto* holder = new QWidget(form->parentWidget());
    auto* holderLayout = new QHBoxLayout(holder);
    holderLayout->setContentsMargins(0, 0, 0, 0);
    holderLayout->setSpacing(2);
    holderLayout->addWidget(field, 1);
    holderLayout->addWidget(key);
    holderLayout->addWidget(expr);

    // Liga o editor a propriedade. Todo caminho passa por withTargetProperty,
    // que resolve o par (composicao, no) - fazer a busca aqui evita repetir
    // esse codigo sete vezes.
    //
    // O valor anterior viaja junto: e o que permite desfazer. A janela monta
    // o comando de desfazer a partir dos dois.
    const auto apply = [this, propName](const Value& v) {
        if (m_updating) return;
        m_updating = true;
        Value before;
        withTargetProperty(propName, [&before](const Property& p, Node&) {
            before = p.baseValue();
        });
        withTargetProperty(propName, [&v](Property& p, Node&) { p.setBaseValue(v); });
        m_updating = false;
        emit propertyChanged(m_target, QString::fromStdString(propName), before, v);
    };

    if (auto* num = dynamic_cast<NumberEditor*>(editor.get())) {
        connect(num, &NumberEditor::valueChanged, this, [apply](double v) { apply(Value(v)); });
        connect(num, &NumberEditor::editingFinished, this,
                [this] { emit keyframeToggled(m_target, QString(), -1.0); });
    } else if (auto* col = dynamic_cast<ColorEditor*>(editor.get())) {
        connect(col, &ColorEditor::valueChanged, this,
                [apply](Color c) { apply(Value(c)); });
    } else if (auto* en = dynamic_cast<EnumEditor*>(editor.get())) {
        connect(en, &EnumEditor::valueChanged, this, [apply](QString s) {
            apply(Value(s.toStdString()));
        });
    } else if (auto* txt = dynamic_cast<TextEditor*>(editor.get())) {
        connect(txt, &TextEditor::valueChanged, this, [apply](QString s) {
            apply(Value(s.toStdString()));
        });
    } else if (auto* b = dynamic_cast<BoolEditor*>(editor.get())) {
        connect(b, &BoolEditor::valueChanged, this, [apply](bool v) { apply(Value(v)); });
    } else if (auto* vec = dynamic_cast<VectorEditor*>(editor.get())) {
        connect(vec, &VectorEditor::valueChanged, this,
                [this, propName](double x, double y, double z) {
                    if (m_updating) return;
                    m_updating = true;
                    Value before;
                    Value after;
                    withTargetProperty(propName, [&](Property& p, Node&) {
                        before = p.baseValue();
                        after = p.type() == ValueType::Vec3 ? Value(Vec3{x, y, z})
                                                            : Value(Vec2{x, y});
                        p.setBaseValue(after);
                    });
                    m_updating = false;
                    emit propertyChanged(m_target, QString::fromStdString(propName),
                                         before, after);
                });
    }

    connect(key, &QToolButton::clicked, this, [this, propName, &row] {
        Property& prop = *row.prop;
        if (!prop.isAnimatable()) {
            emit statusMessage(QStringLiteral("Esta propriedade nao pode ser animada"));
            return;
        }

        const Time now = m_playback.currentTime();
        const bool existed = prop.keyframeIndexAt(now) != kNoKey;

        // A keyframe nasce com o valor que o controle mostra: o que o usuario
        // ve e ve. O comando de desfazer recebe esse valor, entao criar e
        // remover voltam exatamente ao estado anterior.
        Value value;
        if (existed) {
            for (const auto& k : prop.keyframes()) {
                if (std::abs(k.time - now) <= 1e-6) {
                    value = k.value;
                    break;
                }
            }
            prop.removeKeyframeAt(now);
            emit statusMessage(QStringLiteral("Keyframe removida"));
        } else {
            value = row.editor ? row.editor->value() : prop.evaluate(now);
            if (!value.isValid()) value = prop.evaluate(now);
            prop.setKeyframe(now, value);
            emit statusMessage(QStringLiteral("Keyframe criada"));
        }

        updateKeyButton(row);
        emit keyframeToggled(m_target, QString::fromStdString(propName), now, existed,
                             value);
    });

    connect(expr, &QToolButton::clicked, this, [this, &row] {
        if (row.prop->hasExpression()) {
            withTargetProperty(row.name, [](Property& p, Node&) {
                p.setExpression(std::string());
            });
            if (m_expressionRow == &row) hideExpressionEditor();
            updateExprButton(row);
            emit expressionToggled(m_target, QString::fromStdString(row.name));
        } else {
            showExpressionEditor(row);
        }
    });

    auto* label = new QLabel(labelFor(prop), form->parentWidget());
    if (!prop.tooltip().empty()) {
        label->setToolTip(QString::fromStdString(prop.tooltip()));
    }
    form->addRow(label, holder);

    row.editor = std::move(editor);
    row.keyButton = key;
    row.exprButton = expr;
    row.row = holder;
}

InspectorWidget::Row* InspectorWidget::findRow(const std::string& name) {
    for (auto& row : m_rows) {
        if (row->name == name) return row.get();
    }
    return nullptr;
}

void InspectorWidget::refreshValues() {
    m_updating = true;
    for (auto& row : m_rows) {
        if (!row->editor || !row->prop) continue;
        row->editor->setValueQuiet(row->prop->evaluate(m_playback.currentTime()));
    }
    m_updating = false;
}

void InspectorWidget::updateKeyButton(Row& row) {
    auto* button = qobject_cast<QToolButton*>(row.keyButton);
    if (!button || !row.prop) return;

    const bool hasKey = row.prop->keyframeIndexAt(m_playback.currentTime(), 1e-6) != kNoKey;
    button->setText(hasKey ? QStringLiteral("◆") : QStringLiteral("◇"));
    button->setStyleSheet(hasKey ? QStringLiteral("color: #e0b050;")
                                 : QStringLiteral("color: #5b6069;"));
}

void InspectorWidget::updateExprButton(Row& row) {
    auto* button = qobject_cast<QToolButton*>(row.exprButton);
    if (!button || !row.prop) return;
    const bool on = row.prop->hasExpression();
    button->setStyleSheet(on ? QStringLiteral("color: #8ad08a;")
                             : QStringLiteral("color: #5b6069;"));
}

void InspectorWidget::showExpressionEditor(Row& row) {
    hideExpressionEditor();
    m_expressionRow = &row;

    auto* box = new QGroupBox(QStringLiteral("Expressao"), m_content);
    auto* layout = new QVBoxLayout(box);

    auto* editor = new QPlainTextEdit(box);
    editor->setPlainText(QString::fromStdString(row.prop->expression()));
    editor->setFont(monoFont(9));
    editor->setMinimumHeight(70);
    editor->setPlaceholderText(QStringLiteral("wiggle(2, 20)\nloopOut(\"cycle\")\n"
                                             "time * 360"));
    layout->addWidget(editor);

    auto* feedback = new QLabel(QString(), box);
    feedback->setObjectName(QStringLiteral("errorText"));
    feedback->setWordWrap(true);
    layout->addWidget(feedback);

    auto* buttons = new QHBoxLayout();
    auto* apply = new QPushButton(QStringLiteral("Aplicar"), box);
    auto* cancel = new QPushButton(QStringLiteral("Cancelar"), box);
    buttons->addWidget(apply);
    buttons->addWidget(cancel);
    buttons->addStretch(1);
    layout->addLayout(buttons);

    m_layout->insertWidget(std::max(0, m_layout->count() - 1), box);

    const std::string propName = row.name;
    const NodeId nodeId = m_target;

    connect(apply, &QPushButton::clicked, this,
            [this, editor, feedback, propName, nodeId] {
                const std::string source = editor->toPlainText().toStdString();

                // Valida antes de aplicar: Property::evaluate cairia na curva
                // para o frame inteiro, e o usuario nao entenderia por que o
                // valor parou de responder.
                std::string error;
                if (!source.empty() && !expr::Engine::instance().compile(source, &error)) {
                    feedback->setText(QStringLiteral("Erro: %1")
                                          .arg(QString::fromStdString(error)));
                    return;
                }
                feedback->clear();

                withTargetProperty(propName,
                                   [&source](Property& p, Node&) { p.setExpression(source); });
                if (Row* r = findRow(propName)) updateExprButton(*r);
                hideExpressionEditor();
                emit expressionToggled(nodeId, QString::fromStdString(propName));
                emit propertyChanged(nodeId, QString::fromStdString(propName));
            });

    connect(cancel, &QPushButton::clicked, this, &InspectorWidget::hideExpressionEditor);
    editor->setFocus();
}

void InspectorWidget::hideExpressionEditor() {
    if (!m_expressionRow) return;

    // O grupo fica como penultimo item (o ultimo e o stretch).
    const int count = m_layout->count();
    for (int i = count - 2; i >= 0; --i) {
        QLayoutItem* item = m_layout->itemAt(i);
        if (!item || !item->widget()) continue;
        if (qobject_cast<QGroupBox*>(item->widget()) == nullptr) continue;
        if (item->widget()->isVisible() || true) {
            m_layout->removeItem(item);
            item->widget()->deleteLater();
            delete item;
            break;
        }
    }
    m_expressionRow = nullptr;
}

}  // namespace lmn::ui
