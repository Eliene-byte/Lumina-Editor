// InspectorWidget.h - Painel de propriedades do no selecionado.
//
// Monta os controles a partir do NodeTypeDesc, entao um no novo aparece no
// inspector sem nenhuma linha de codigo de UI. Cada linha tem o botao de
// keyframe e o de expressao ao lado do valor - o trio que define como se
// anima no After Effects.
#pragma once

#include <map>
#include <memory>
#include <vector>

#include <QWidget>

#include "core/Project.h"
#include "ui/PlaybackController.h"
#include "ui/Theme.h"

class QFormLayout;
class QScrollArea;
class QVBoxLayout;

namespace lmn::ui {

// Um editor para um tipo de valor especifico. Devolvem o valor ja convertido.
class ValueEditor {
public:
    virtual ~ValueEditor() = default;
    virtual QWidget* widget() = 0;
    // Escreve o valor no controle sem emitir sinal (atualizacao externa).
    virtual void setValueQuiet(const Value& v) = 0;
    [[nodiscard]] virtual Value value() const = 0;
};

class InspectorWidget : public QWidget {
    Q_OBJECT
public:
    explicit InspectorWidget(Project& project, PlaybackController& playback,
                            QWidget* parent = nullptr);
    ~InspectorWidget() override;

    // O alvo e o par (composicao, no). NodeId sozinho nao identifica um no:
    // cada grafo numera seus nos a partir de 1, entao o id 1 existe em todas
    // as composicoes.
    void setTarget(CompId comp, NodeId id);
    [[nodiscard]] CompId targetComposition() const { return m_targetComp; }
    [[nodiscard]] NodeId target() const { return m_target; }
    void clear();

    // Filtra as propriedades visiveis (usado pelo campo de busca).
    void setFilter(const QString& text);
    // Mostra so as propriedades com animacao.
    void setShowAnimatedOnly(bool on);

    void setUndoAvailable(bool canUndo, bool canRedo);

signals:
    // Alteracao simples de valor. Carrega o valor anterior para que a janela
    // possa montar um comando de desfazer: sem ele, o usuario perderia o estado
    // antigo assim que o controle mudasse.
    void propertyChanged(NodeId node, const QString& property, Value before, Value after);
    void keyframeToggled(NodeId node, const QString& property, Time time, bool existed,
                         Value value);
    void expressionToggled(NodeId node, const QString& property);
    void statusMessage(QString text);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Row {
        std::string name;
        Property* prop = nullptr;
        std::unique_ptr<ValueEditor> editor;
        QWidget* keyButton = nullptr;
        QWidget* exprButton = nullptr;
        QWidget* row = nullptr;
    };

    // Cria o editor, os botoes e liga os sinais de uma propriedade.
    void buildRow(QFormLayout* form, Row& row);

    void rebuild();
    void clearRows();
    [[nodiscard]] Row* findRow(const std::string& name);
    void refreshValues();
    void updateKeyButton(Row& row);
    void updateExprButton(Row& row);
    void showNodeHeader(const Node& node);
    void showExpressionEditor(Row& row);
    void hideExpressionEditor();
    [[nodiscard]] bool matchesFilter(const Property& prop) const;
    // Agrupa as propriedades por "group" do spec, na ordem em que aparecem.
    [[nodiscard]] std::vector<std::pair<std::string, std::vector<Row*>>> groupRows(
        std::vector<Row>& rows);
    // Aplica uma mudanca na propriedade alvo sem repetir a busca do no.
    template <typename Fn>
    void withTargetProperty(const std::string& name, Fn&& fn) {
        if (m_targetComp == 0) return;
        Composition* comp = m_project.composition(m_targetComp);
        if (!comp) return;
        Node* node = comp->graph().node(m_target);
        if (!node) return;
        if (Property* p = node->findProperty(name)) fn(*p, *node);
    }

    Project& m_project;
    PlaybackController& m_playback;
    Palette m_palette;

    CompId m_targetComp = 0;
    NodeId m_target = kInvalidId;
    std::string m_targetType;   // tipo do no em edicao, para achar os specs
    QScrollArea* m_scroll = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QWidget* m_content = nullptr;
    std::vector<std::unique_ptr<Row>> m_rows;
    Row* m_expressionRow = nullptr;
    QString m_filter;
    bool m_animatedOnly = false;
    bool m_updating = false;
    bool m_undoAvailable = false;
    bool m_redoAvailable = false;
};

}  // namespace lmn::ui
