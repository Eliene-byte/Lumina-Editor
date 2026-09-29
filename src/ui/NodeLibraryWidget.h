// NodeLibraryWidget.h - Biblioteca de nos com busca.
//
// Um QTreeWidget: categorias no primeiro nivel, tipos no segundo. Busca por
// nome, categoria e ate por propriedade ("nos que tem raio" encontra o desfoque
// e a vinheta). O duplo clique cria o no na posicao do cursor.
#pragma once

#include <QWidget>

#include "core/Project.h"
#include "ui/PlaybackController.h"

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace lmn::ui {

class NodeLibraryWidget : public QWidget {
    Q_OBJECT
public:
    explicit NodeLibraryWidget(Project& project, PlaybackController& playback,
                               QWidget* parent = nullptr);
    ~NodeLibraryWidget() override;

    void setFilter(const QString& text);
    [[nodiscard]] QString filter() const;

    // Destaca um tipo (usado quando o usuario seleciona um no e quer achar o
    // mesmo tipo na biblioteca).
    void highlightType(const std::string& type);

    // Numero de tipos visiveis com o filtro atual.
    [[nodiscard]] int visibleTypeCount() const;

signals:
    // O usuario pediu para criar um no deste tipo.
    void nodeRequested(const std::string& type);
    // Duplo clique em um no ja presente no grafo: revela no editor de grafos.
    void revealRequested(NodeId nodeId);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void buildTree();
    void applyFilter(const QString& text);
    void addNodeFromItem(QTreeWidgetItem* item);
    [[nodiscard]] QTreeWidgetItem* itemForType(const std::string& type) const;
    // Uma propriedade casa quando o termo aparece no nome, na dica ou no
    // rotulo do grupo. E o que faz "raio" achar o desfoque e a vinheta.
    static bool matchesQuery(const NodeTypeDesc& desc, const QString& query);

    Project& m_project;
    PlaybackController& m_playback;
    QLineEdit* m_search = nullptr;
    QTreeWidget* m_tree = nullptr;
    std::map<std::string, QTreeWidgetItem*> m_items;
};

}  // namespace lmn::ui
