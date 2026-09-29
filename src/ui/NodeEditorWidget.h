// NodeEditorWidget.h - Editor de grafos de nos.
//
// Tudo desenhado com QPainter em um unico widget. A alternativa (um QWidget
// por no) cria centenas de widgets e fica lenta assim que o grafo passa de
// 20 nos - exatamente o caso de uso deste editor.
#pragma once

#include <memory>
#include <vector>

#include <QPointF>
#include <QRectF>
#include <QWidget>

#include "core/Project.h"
#include "ui/PlaybackController.h"
#include "ui/Theme.h"

namespace lmn::ui {

class NodeEditorWidget : public QWidget {
    Q_OBJECT
public:
    enum class Tool { Select, Hand, Wire };

    explicit NodeEditorWidget(Project& project, PlaybackController& playback,
                              QWidget* parent = nullptr);
    ~NodeEditorWidget() override;

    // --- Composicao -----------------------------------------------------------
    void setComposition(CompId id);
    [[nodiscard]] CompId composition() const { return m_comp; }

    // --- Selecao --------------------------------------------------------------
    void selectNode(NodeId id, bool addToSelection = false);
    void clearSelection();
    [[nodiscard]] NodeId selectedNode() const { return m_singleSelection; }
    [[nodiscard]] const std::vector<NodeId>& selection() const { return m_selection; }

    // --- Camera ---------------------------------------------------------------
    void fitToGraph();
    void frameNode(NodeId id);
    [[nodiscard]] double zoom() const { return m_zoom; }

    // --- Ferramentas ----------------------------------------------------------
    void setTool(Tool t) { m_tool = t; }
    [[nodiscard]] Tool tool() const { return m_tool; }

    // Faz a proxima conexao partir da entrada de um no. Usado pelo duplo
    // clique no no de saida na biblioteca.
    void beginConnection(NodeId from, int outputIndex);
    void cancelConnection();

    // --- Edicao ---------------------------------------------------------------
    Node* addNode(const std::string& type, const QPointF& at);
    void deleteSelection();
    void duplicateSelection();
    void selectAll();
    void groupSelection();
    void ungroupSelection();
    // Traz o no selecionado para frente da ordem de mesclagem.
    void bringToFront();
    void sendToBack();
    void toggleEnabled();
    void toggleSolo();
    void toggleLock();

    // --- Desfazer -------------------------------------------------------------
    // Os comandos ficam no CoreController; aqui so pedimos.
    void setUndoAvailable(bool canUndo, bool canRedo);

signals:
    void selectionChanged();
    void nodeDoubleClicked(NodeId id);
    void graphChanged();
    void statusMessage(QString text);
    void contextMenuRequested(NodeId id, QPoint globalPos);
    // O no em edicao de expressao precisa que o inspector saiba qual e.
    void editingChanged(NodeId id);

    // Cada mudanca estrutural viaja com o estado necessario para desfazer.
    // Sem isso, a janela teria de adivinhar o que mudou e o desfazer voltaria
    // a um estado proximo, nao ao estado exato.
    void nodeAdded(CompId comp, Node node);
    void nodeRemoved(CompId comp, Node node);
    void connectionChanged(CompId comp, NodeId from, int fromOutput, NodeId to,
                           int toInput, NodeId previousSource);
    void nodeEnabledChanged(CompId comp, NodeId node, bool enabled);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    struct Geometry {
        QRectF box;                 // retangulo do no, em coordenadas de tela
        QRectF header;              // faixa do nome
        QPointF outputPort;         // ponto de saida
        QPointF previewTopLeft;
        QPointF previewBottomRight;
        [[nodiscard]] QPointF inputPort(int index, int count) const;
    };

    [[nodiscard]] Geometry geometryFor(const Node& node) const;
    [[nodiscard]] QPointF graphToScreen(const QPointF& p) const;
    [[nodiscard]] QPointF screenToGraph(const QPointF& p) const;
    [[nodiscard]] NodeId nodeAt(const QPointF& screenPos) const;
    [[nodiscard]] int portAt(const QPointF& screenPos, NodeId& node, bool& isInput,
                             int& index) const;
    [[nodiscard]] bool isSelected(NodeId id) const;
    void selectInBox(const QRectF& box, bool additive);
    void moveSelection(const QPointF& delta);
    void snapToGrid(QPointF& pos) const;
    [[nodiscard]] QPointF snapValue(const QPointF& p) const;
    [[nodiscard]] bool onGrid(const QPointF& p) const;
    [[nodiscard]] NodeId hoveredNode() const { return m_hoverNode; }
    void updateHover(const QPointF& pos);
    [[nodiscard]] Node* selectedSingleNode();

    void paintGrid(QPainter& p);
    void paintWires(QPainter& p);
    void paintNode(QPainter& p, const Node& node, const Geometry& geo);
    void paintWirePreview(QPainter& p);
    void paintHeader(QPainter& p);

    Project& m_project;
    PlaybackController& m_playback;
    CompId m_comp = 0;

    Palette m_palette;
    Metrics m_metrics;

    // Camera: deslocamento em pixels de tela e zoom.
    QPointF m_pan;
    double m_zoom = 1.0;

    std::vector<NodeId> m_selection;
    NodeId m_singleSelection = kInvalidId;
    NodeId m_hoverNode = kInvalidId;
    NodeId m_dragNode = kInvalidId;
    NodeId m_editingNode = kInvalidId;

    // Arraste
    enum class DragMode { None, Pan, Nodes, Wire, BoxSelect, Resize };
    DragMode m_dragMode = DragMode::None;
    QPointF m_dragStartScreen;
    QPointF m_dragStartGraph;
    QPointF m_dragLast;
    QRectF m_selectionBox;

    // Conexao em andamento
    bool m_wiring = false;
    NodeId m_wireFrom = kInvalidId;
    int m_wireOutput = 0;
    bool m_wireToInput = true;    // arrastando de saida para entrada
    QPointF m_wireCursor;

    Tool m_tool = Tool::Select;
    int m_resizeCorner = -1;
    bool m_snap = true;
    bool m_showPreviews = true;
    bool m_showPasses = false;
    bool m_undoAvailable = false;
    bool m_redoAvailable = false;
};

}  // namespace lmn::ui
