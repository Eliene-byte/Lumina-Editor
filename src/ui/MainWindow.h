// MainWindow.h - Monta e conecta tudo.
//
// Regra deste arquivo: MainWindow nao decide nada de edicao. Ele traduz acoes
// da interface em chamadas sobre o Project e conecta sinais. Quem guarda
// estado e o Project; quem decide e o grafo. Se uma regra de negocio aparecer
// aqui, ela esta no lugar errado.
#pragma once

#include <memory>

#include <QMainWindow>
#include <QUndoCommand>
#include <QUndoStack>

#include "core/Project.h"
#include "media/MediaBackend.h"
#include "media/SourceProviderGL.h"
#include "ui/PlaybackController.h"

class QComboBox;
class QDockWidget;
class QLabel;
class QSlider;
class QSplitter;
class QTabWidget;
class QToolButton;

namespace lmn::ui {

class InspectorWidget;
class NodeEditorWidget;
class NodeLibraryWidget;
class TimelineWidget;
class ViewerWidget;

// ---------------------------------------------------------------------------
// Comandos de desfazer
//
// Um comando por acao. Copiar o Project inteiro a cada passo custaria centenas
// de MB em um projeto real e travaria a interface, entao cada comando guarda
// so o que mudou.
// ---------------------------------------------------------------------------

// Troca o valor de uma propriedade, com desfazer do valor anterior.
class SetPropertyCommand : public QUndoCommand {
public:
    SetPropertyCommand(Project& project, CompId comp, NodeId node,
                       std::string property, Value before, Value after,
                       QString label);
    void undo() override;
    void redo() override;

private:
    void apply(const Value& v);

    Project& m_project;
    CompId m_comp;
    NodeId m_node;
    std::string m_property;
    Value m_before;
    Value m_after;
};

// Cria ou apaga uma keyframe.
class KeyframeCommand : public QUndoCommand {
public:
    KeyframeCommand(Project& project, CompId comp, NodeId node,
                    std::string property, Time time, bool existed, Value value,
                    QString label);
    void undo() override;
    void redo() override;

private:
    Project& m_project;
    CompId m_comp;
    NodeId m_node;
    std::string m_property;
    Time m_time;
    bool m_existed;
    Value m_value;
};

// Adiciona um no ao grafo.
class AddNodeCommand : public QUndoCommand {
public:
    AddNodeCommand(Project& project, CompId comp, Node node, int inputSlot, NodeId from);
    void undo() override;
    void redo() override;

private:
    Project& m_project;
    CompId m_comp;
    Node m_node;
    int m_slot;
    NodeId m_from;
};

// Remove um no, guardando uma copia completa para desfazer.
class RemoveNodeCommand : public QUndoCommand {
public:
    RemoveNodeCommand(Project& project, CompId comp, Node node, int inputSlot, NodeId from);
    void undo() override;
    void redo() override;

private:
    Project& m_project;
    CompId m_comp;
    Node m_node;
    int m_slot;
    NodeId m_from;
};

// Liga (ou religa) duas entradas.
class ConnectCommand : public QUndoCommand {
public:
    ConnectCommand(Project& project, CompId comp, NodeId from, int fromOut, NodeId to,
                   int toIn, NodeId previousSource, QString label);
    void undo() override;
    void redo() override;

private:
    Project& m_project;
    CompId m_comp;
    NodeId m_from;
    int m_fromOut;
    NodeId m_to;
    int m_toIn;
    NodeId m_previousSource;
};

// Liga ou desliga um no.
class ToggleNodeCommand : public QUndoCommand {
public:
    ToggleNodeCommand(Project& project, CompId comp, NodeId node, bool before,
                      QString label);
    void undo() override;
    void redo() override;

private:
    Project& m_project;
    CompId m_comp;
    NodeId m_node;
    bool m_before;
};

// ---------------------------------------------------------------------------
// MainWindow
// ---------------------------------------------------------------------------

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Sobe o motor de expressoes, registra os nos e monta a interface.
    void bootstrap();

    bool openProject(const QString& path);
    bool saveProject();
    bool saveProjectAs();

    // Modo leve: metade da resolucao, cache menor, proxies. E o caminho para
    // o editor continuar usavel num PC sem GPU dedicada.
    void setLightweightMode(bool on);
    [[nodiscard]] bool lightweightMode() const { return m_lightweight; }

    [[nodiscard]] Project& project() { return m_project; }
    [[nodiscard]] PlaybackController& playback() { return m_playback; }

    // Usado pelo dialogo de exportacao.
    [[nodiscard]] NodeEditorWidget* nodeEditor() const { return m_nodes.get(); }
    [[nodiscard]] ViewerWidget* viewer() const { return m_viewer.get(); }

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNewProject();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onExportFrame();
    void onExportImage();
    void onExportMovie();

    void onAddNodeRequested(const std::string& type);
    // Slots dos sinais de mudanca estrutural do editor de nos. Cada um vira um
    // comando na pilha de desfazer.
    void onNodeAdded(CompId comp, Node node);
    void onNodeRemoved(CompId comp, Node node);
    void onConnectionChanged(CompId comp, NodeId from, int fromOutput, NodeId to,
                             int toInput, NodeId previousSource);
    void onNodeEnabledChanged(CompId comp, NodeId node, bool was);
    void onSelectionChanged();
    void onTimeChanged(double time);
    void onGraphChanged();
    void onPropertyChanged(NodeId node, const QString& property, Value before, Value after);
    void onKeyframeToggled(NodeId node, const QString& property, double time,
                           bool existed, Value value);
    void onExpressionToggled(NodeId node, const QString& property);
    void onClipDoubleClicked(ClipId id);

    void onUndo();
    void onRedo();
    void onToggleLightweight();
    void onQualityChanged(int level);

public slots:
    // Seta o nivel de qualidade (0 leve, 1 normal, 2 alto). Usado pela linha de
    // comando e pelo modo leve automatico.
    void setQuality(int level);

private:
    void createActions();
    void createMenus();
    void createToolBar();
    void createDocks();
    void createTransportBar();
    void connectSignals();
    void applyTheme();
    void setWindowTitleFromProject();
    void setDirty(bool dirty);
    void pushCommand(QUndoCommand* command);
    // Encaminha um comando para a pilha, ja marcando o projeto como sujo.
    void commit(QUndoCommand* command);

    void refreshCompositionList();
    void refreshUndoState();
    void updateStatusBar();
    void showStatus(const QString& text, int timeoutMs = 3000);
    void syncSelectionToInspector();
    void refreshAll();
    // Mover o no recem-criado para onde o usuario espera: um pouco a direita
    // e abaixo do no selecionado, sem cobrir o existente.
    [[nodiscard]] QPointF spawnPositionForNewNode() const;

    Project m_project;
    media::MediaBackend* m_backend = nullptr;
    std::unique_ptr<media::SourceProviderGL> m_sources;

    PlaybackController m_playback;
    std::unique_ptr<QUndoStack> m_undo;

    std::unique_ptr<ViewerWidget> m_viewer;
    std::unique_ptr<NodeEditorWidget> m_nodes;
    std::unique_ptr<TimelineWidget> m_timeline;
    std::unique_ptr<InspectorWidget> m_inspector;
    std::unique_ptr<NodeLibraryWidget> m_library;

    QSplitter* m_center = nullptr;
    QTabWidget* m_bottomTabs = nullptr;
    QComboBox* m_compositionList = nullptr;
    QSlider* m_qualitySlider = nullptr;
    QToolButton* m_lightButton = nullptr;
    QLabel* m_statusRight = nullptr;
    QLabel* m_perfLabel = nullptr;
    QLabel* m_timecodeLabel = nullptr;

    bool m_lightweight = false;
    bool m_dirty = false;
    bool m_loading = false;
};

}  // namespace lmn::ui
