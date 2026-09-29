#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>

#include "InspectorWidget.h"
#include "NodeEditorWidget.h"
#include "NodeLibraryWidget.h"
#include "Theme.h"
#include "TimelineWidget.h"
#include "ViewerWidget.h"
#include "core/NodeRegistry.h"
#include "expr/Expression.h"
#include "io/ProjectSerializer.h"
#include "media/FrameExporter.h"
#include "nodes/ImageCache.h"
#include "nodes/Nodes.h"

namespace lmn::ui {
namespace {

constexpr uint32_t kNoKey = std::numeric_limits<uint32_t>::max();

QString formatTimecode(Time t, double fps) {
    const int totalFrames = static_cast<int>(std::llround(t * fps));
    const int rate = std::max(1, static_cast<int>(fps));
    const int f = totalFrames % rate;
    const int totalSeconds = totalFrames / rate;
    return QStringLiteral("%1:%2:%3")
        .arg(totalSeconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((totalSeconds / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(totalSeconds % 60, 2, 10, QLatin1Char('0'))
        .arg(f, 2, 10, QLatin1Char('0'));
}

}  // namespace

// ---------------------------------------------------------------------------
// Comandos
// ---------------------------------------------------------------------------

SetPropertyCommand::SetPropertyCommand(Project& project, CompId comp, NodeId node,
                                       std::string property, Value before, Value after,
                                       QString label)
    : QUndoCommand(std::move(label)), m_project(project), m_comp(comp), m_node(node),
      m_property(std::move(property)), m_before(std::move(before)),
      m_after(std::move(after)) {}

void SetPropertyCommand::apply(const Value& v) {
    Composition* comp = m_project.composition(m_comp);
    if (!comp) return;
    Node* node = comp->graph().node(m_node);
    if (!node) return;
    if (Property* prop = node->findProperty(m_property)) prop->setBaseValue(v);
    m_project.touch();
}

void SetPropertyCommand::undo() { apply(m_before); }
void SetPropertyCommand::redo() { apply(m_after); }

KeyframeCommand::KeyframeCommand(Project& project, CompId comp, NodeId node,
                                 std::string property, Time time, bool existed,
                                 Value value, QString label)
    : QUndoCommand(std::move(label)), m_project(project), m_comp(comp), m_node(node),
      m_property(std::move(property)), m_time(time), m_existed(existed),
      m_value(std::move(value)) {}

void KeyframeCommand::redo() {
    Composition* comp = m_project.composition(m_comp);
    if (!comp) return;
    Node* node = comp->graph().node(m_node);
    if (!node) return;
    if (Property* prop = node->findProperty(m_property)) {
        if (m_existed) {
            prop->setKeyframe(m_time, m_value);
        } else {
            prop->removeKeyframeAt(m_time);
        }
    }
    m_project.touch();
}

void KeyframeCommand::undo() {
    Composition* comp = m_project.composition(m_comp);
    if (!comp) return;
    Node* node = comp->graph().node(m_node);
    if (!node) return;
    if (Property* prop = node->findProperty(m_property)) {
        if (m_existed) {
            prop->removeKeyframeAt(m_time);
        } else {
            prop->setKeyframe(m_time, m_value);
        }
    }
    m_project.touch();
}

AddNodeCommand::AddNodeCommand(Project& project, CompId comp, Node node, int inputSlot,
                               NodeId from)
    : QUndoCommand(QStringLiteral("adicionar no")), m_project(project), m_comp(comp),
      m_node(std::move(node)), m_slot(inputSlot), m_from(from) {}

void AddNodeCommand::redo() {
    Composition* comp = m_project.composition(m_comp);
    if (!comp) return;
    if (Node* added = comp->graph().addNode(m_node)) {
        if (m_from != kInvalidId) added->setInput(m_slot, m_from);
    }
    m_project.touch();
}

void AddNodeCommand::undo() {
    if (Composition* comp = m_project.composition(m_comp)) {
        comp->graph().removeNode(m_node.id());
    }
    m_project.touch();
}

RemoveNodeCommand::RemoveNodeCommand(Project& project, CompId comp, Node node,
                                     int inputSlot, NodeId from)
    : QUndoCommand(QStringLiteral("remover no")), m_project(project), m_comp(comp),
      m_node(std::move(node)), m_slot(inputSlot), m_from(from) {}

void RemoveNodeCommand::redo() {
    if (Composition* comp = m_project.composition(m_comp)) {
        comp->graph().removeNode(m_node.id());
    }
    m_project.touch();
}

void RemoveNodeCommand::undo() {
    Composition* comp = m_project.composition(m_comp);
    if (!comp) return;
    if (Node* added = comp->graph().addNode(m_node)) {
        if (m_from != kInvalidId) added->setInput(m_slot, m_from);
    }
    m_project.touch();
}

ConnectCommand::ConnectCommand(Project& project, CompId comp, NodeId from, int fromOut,
                               NodeId to, int toIn, NodeId previousSource,
                               QString label)
    : QUndoCommand(std::move(label)), m_project(project), m_comp(comp), m_from(from),
      m_fromOut(fromOut), m_to(to), m_toIn(toIn), m_previousSource(previousSource) {}

void ConnectCommand::redo() {
    if (Composition* comp = m_project.composition(m_comp)) {
        comp->graph().connect(m_from, m_fromOut, m_to, m_toIn);
    }
    m_project.touch();
}

void ConnectCommand::undo() {
    Composition* comp = m_project.composition(m_comp);
    if (!comp) return;
    if (m_previousSource == kInvalidId) {
        comp->graph().disconnect(m_to, m_toIn);
    } else {
        comp->graph().connect(m_previousSource, 0, m_to, m_toIn);
    }
    m_project.touch();
}

ToggleNodeCommand::ToggleNodeCommand(Project& project, CompId comp, NodeId node,
                                     bool before, QString label)
    : QUndoCommand(std::move(label)), m_project(project), m_comp(comp), m_node(node),
      m_before(before) {}

void ToggleNodeCommand::redo() {
    if (Composition* comp = m_project.composition(m_comp)) {
        if (Node* n = comp->graph().node(m_node)) n->setEnabled(!m_before);
    }
    m_project.touch();
}

void ToggleNodeCommand::undo() {
    if (Composition* comp = m_project.composition(m_comp)) {
        if (Node* n = comp->graph().node(m_node)) n->setEnabled(m_before);
    }
    m_project.touch();
}

// ---------------------------------------------------------------------------
// MainWindow
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), m_playback(m_project) {
    m_undo = std::make_unique<QUndoStack>();
    resize(1600, 940);
}

MainWindow::~MainWindow() = default;

void MainWindow::bootstrap() {
    // A ordem importa: o motor de expressoes antes dos nos (alguns registram
    // funcoes nativas), e o backend de midia antes do provedor de fontes.
    expr::installAsCoreBackend();
    nodes::registerAllNodes();
    m_backend = &media::backend();

    m_project.createDefaultComposition();
    m_project.timeline().addStack("V1");
    m_project.timeline().addStack("V2");
    m_project.timeline().addStack("A1");

    m_sources = std::make_unique<media::SourceProviderGL>(m_project, *m_backend);
    m_sources->initialize();

    createDocks();
    createActions();
    createMenus();
    createToolBar();
    createTransportBar();
    connectSignals();
    applyTheme();

    refreshCompositionList();
    refreshUndoState();
    setWindowTitleFromProject();
    updateStatusBar();

    const auto caps = m_backend->capabilities();
    if (!caps.available) {
        showStatus(
            QStringLiteral("FFmpeg indisponivel: o grafo funciona, mas midia externa "
                           "nao carrega"),
            0);
    }
    if (m_viewer && m_viewer->isSoftwareRenderer()) {
        showStatus(QStringLiteral("GPU por software detectada - modo leve recomendado"),
                   0);
    }
}

void MainWindow::applyTheme() {
    QFile file(QStringLiteral(":/themes/dark.qss"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qApp->setStyleSheet(QString::fromUtf8(file.readAll()));
    }
}

void MainWindow::createDocks() {
    // --- Centro: barra de transporte + visualizador, e a area de trabalho ---
    //
    // A barra de transporte fica ACIMA do visualizador porque e onde o olho
    // ja esta durante a reproducao. A area de trabalho (nos e timeline) fica
    // abaixo, em abas, e o usuario escolhe o que precisa ver.
    m_viewer = std::make_unique<ViewerWidget>(m_project, m_playback, m_sources.get());
    m_viewer->setObjectName(QStringLiteral("viewer"));

    m_nodes = std::make_unique<NodeEditorWidget>(m_project, m_playback);
    m_timeline = std::make_unique<TimelineWidget>(m_project, m_playback);

    m_bottomTabs = new QTabWidget();
    m_bottomTabs->setObjectName(QStringLiteral("bottomTabs"));
    m_bottomTabs->addTab(m_nodes.get(), QStringLiteral("Nos"));
    m_bottomTabs->addTab(m_timeline.get(), QStringLiteral("Linha do tempo"));

    // Superior: visualizador embrulhado (a barra entra depois, em
    // createTransportBar, com insertWidget no indice 0).
    auto* viewerHolder = new QWidget();
    auto* viewerLayout = new QVBoxLayout(viewerHolder);
    viewerLayout->setContentsMargins(0, 0, 0, 0);
    viewerLayout->setSpacing(0);
    viewerLayout->addWidget(m_viewer.get());

    m_center = new QSplitter(Qt::Vertical, this);
    m_center->addWidget(viewerHolder);
    m_center->addWidget(m_bottomTabs);
    m_center->setStretchFactor(0, 3);
    m_center->setStretchFactor(1, 2);
    setCentralWidget(m_center);

    // --- Biblioteca (esquerda) e Inspector (direita) ---
    m_library = std::make_unique<NodeLibraryWidget>(m_project, m_playback);
    auto* libraryDock = new QDockWidget(QStringLiteral("Nos"), this);
    libraryDock->setObjectName(QStringLiteral("libraryDock"));
    libraryDock->setWidget(m_library.get());
    addDockWidget(Qt::LeftDockWidgetArea, libraryDock);
    libraryDock->setMinimumWidth(200);

    m_inspector = std::make_unique<InspectorWidget>(m_project, m_playback);
    auto* inspectorDock = new QDockWidget(QStringLiteral("Propriedades"), this);
    inspectorDock->setObjectName(QStringLiteral("inspectorDock"));
    inspectorDock->setWidget(m_inspector.get());
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);
    inspectorDock->setMinimumWidth(260);
}

void MainWindow::createActions() {
    // Acoes com atalho que nao pertencem a um menu especifico vivem aqui,
    // e nao espalhadas em widgets. O atalho e a acao ficam juntos para os
    // dois nao divergirem quando um deles mudar.
    auto* showLibrary = new QAction(QStringLiteral("Painel de nos"), this);
    showLibrary->setObjectName(QStringLiteral("actionShowLibrary"));
    showLibrary->setCheckable(true);
    connect(showLibrary, &QAction::toggled, this, [this](bool on) {
        if (auto* dock = findChild<QDockWidget*>(QStringLiteral("libraryDock"))) {
            dock->setVisible(on);
        }
    });
    addAction(showLibrary);

    auto* showInspector = new QAction(QStringLiteral("Painel de propriedades"), this);
    showInspector->setObjectName(QStringLiteral("actionShowInspector"));
    showInspector->setCheckable(true);
    connect(showInspector, &QAction::toggled, this, [this](bool on) {
        if (auto* dock = findChild<QDockWidget*>(QStringLiteral("inspectorDock"))) {
            dock->setVisible(on);
        }
    });
    addAction(showInspector);

    auto* showWorkspace = new QAction(QStringLiteral("Area de trabalho"), this);
    showWorkspace->setObjectName(QStringLiteral("actionShowWorkspace"));
    showWorkspace->setCheckable(true);
    showWorkspace->setChecked(true);
    connect(showWorkspace, &QAction::toggled, this,
            [this](bool on) { m_bottomTabs->setVisible(on); });
    addAction(showWorkspace);
}

void MainWindow::createMenus() {
    QMenu* file = menuBar()->addMenu(QStringLiteral("&Projeto"));
    file->addAction(QStringLiteral("&Novo"), QKeySequence::New, this,
                    &MainWindow::onNewProject);
    file->addAction(QStringLiteral("&Abrir..."), QKeySequence::Open, this,
                    &MainWindow::onOpen);
    file->addAction(QStringLiteral("&Salvar"), QKeySequence::Save, this,
                    &MainWindow::onSave);
    file->addAction(QStringLiteral("Salvar &como..."), QKeySequence::SaveAs, this,
                    &MainWindow::onSaveAs);
    file->addSeparator();
    QMenu* exportMenu = file->addMenu(QStringLiteral("&Exportar"));
    exportMenu->addAction(QStringLiteral("&Quadro atual..."), this,
                          &MainWindow::onExportFrame);
    exportMenu->addAction(QStringLiteral("&Imagem..."), this, &MainWindow::onExportImage);
    exportMenu->addAction(QStringLiteral("&Video..."),
                          QKeySequence(QStringLiteral("Ctrl+M")), this,
                          &MainWindow::onExportMovie);
    file->addSeparator();
    file->addAction(QStringLiteral("&Sair"), QKeySequence::Quit, this, &QWidget::close);

    QMenu* edit = menuBar()->addMenu(QStringLiteral("&Editar"));
    edit->addAction(QStringLiteral("&Desfazer"), QKeySequence::Undo, this,
                    &MainWindow::onUndo);
    edit->addAction(QStringLiteral("&Refazer"), QKeySequence::Redo, this,
                    &MainWindow::onRedo);

    QMenu* compMenu = menuBar()->addMenu(QStringLiteral("&Composicao"));
    compMenu->addAction(QStringLiteral("&Nova composicao"), this, [this] {
        const CompId id = m_project.createComposition(
            QStringLiteral("Composicao %1")
                .arg(m_project.compositionCount() + 1)
                .toStdString(),
            m_project.size(), m_project.fps(), 5.0);
        refreshCompositionList();
        m_nodes->setComposition(id);
        m_viewer->setViewedComposition(id);
        setDirty(true);
    });
    compMenu->addAction(QStringLiteral("&Duplicar atual"), this, [this] {
        const CompId id = m_project.duplicateComposition(m_nodes->composition());
        refreshCompositionList();
        if (id) m_nodes->setComposition(id);
        setDirty(true);
    });
    compMenu->addAction(QStringLiteral("&Remover atual"), this, [this] {
        if (m_project.compositionCount() <= 1) {
            showStatus(QStringLiteral("O projeto precisa de ao menos uma composicao"));
            return;
        }
        m_project.removeComposition(m_nodes->composition());
        refreshCompositionList();
        m_nodes->setComposition(m_project.compositions().begin()->first);
        m_viewer->setViewedComposition(m_project.compositions().begin()->first);
        setDirty(true);
    });

    QMenu* timelineMenu = menuBar()->addMenu(QStringLiteral("&Linha do tempo"));
    timelineMenu->addAction(QStringLiteral("&Dividir no cursor (K)"),
                            QKeySequence(QStringLiteral("K")), this,
                            [this] { m_timeline->splitAtPlayhead(); });
    timelineMenu->addAction(QStringLiteral("&Ripple delete (Shift+Del)"), this,
                            [this] { m_timeline->rippleDeleteSelection(); });
    timelineMenu->addAction(QStringLiteral("&Ajustar ao conteudo"), this,
                            [this] { m_timeline->zoomToFit(); });

    QMenu* view = menuBar()->addMenu(QStringLiteral("&Exibir"));

    // Atalhos para os paineis: F5 (nos), F6 (propriedades), F7 (area). Sao
    // atalhos de painel de producao, e nao de edicao - por isso nao usam
    // Ctrl e nao conflitam com salvar/abrir.
    auto* panels = view->addMenu(QStringLiteral("&Paineis"));
    for (const QString& name : {QStringLiteral("actionShowLibrary"),
                                QStringLiteral("actionShowInspector"),
                                QStringLiteral("actionShowWorkspace")}) {
        if (auto* action = findChild<QAction*>(name)) {
            panels->addAction(action);
        }
    }
    panels->addSeparator();
    view->addAction(m_bottomTabs, QStringLiteral("&Area de trabalho"));
    view->addAction(m_nodes, QStringLiteral("&Editor de nos"));
    view->addAction(m_timeline, QStringLiteral("&Linha do tempo"));
    view->addSeparator();
    view->addAction(QStringLiteral("Modo &leve"),
                    QKeySequence(QStringLiteral("Ctrl+L")), this,
                    &MainWindow::onToggleLightweight);

    QMenu* help = menuBar()->addMenu(QStringLiteral("A&juda"));
    help->addAction(QStringLiteral("Atalhos"), this, [this] {
        QMessageBox::information(
            this, QStringLiteral("Atalhos"),
            QStringLiteral(
                "<table>"
                "<tr><td><b>Espaco</b></td><td>reproduzir / pausar</td></tr>"
                "<tr><td><b>Ctrl+L</b></td><td>modo leve</td></tr>"
                "<tr><td><b>K</b></td><td>dividir clip no cursor</td></tr>"
                "<tr><td><b>I / O</b></td><td>marcar entrada / saida</td></tr>"
                "<tr><td><b>Del</b></td><td>apagar selecao</td></tr>"
                "<tr><td><b>Ctrl+Z</b></td><td>desfazer</td></tr>"
                "<tr><td><b>Ctrl+D</b></td><td>duplicar nos</td></tr>"
                "<tr><td><b>Shift+arrastar</b></td><td>adicionar a selecao</td></tr>"
                "<tr><td><b>Ctrl+arrastar</b></td><td>zoom no visualizador</td></tr>"
                "</table>"));
    });
    help->addAction(QStringLiteral("Sobre"), this, [this] {
        QMessageBox::about(
            this, QStringLiteral("Sobre"),
            QStringLiteral(
                "<b>Lumina</b><p>Editor de video portatil: composicao por nos, "
                "linha do tempo multipista e correcao de cor num unico executavel.</p>"
                "<p>GPU: %1</p><p>Midia: %2</p><p>Tipos de no: %3</p>")
                .arg(m_viewer ? m_viewer->rendererName() : QStringLiteral("?"),
                     m_backend ? m_backend->name() : QStringLiteral("?"))
                .arg(nodes::registeredNodeTypeCount()));
    });
}

void MainWindow::createToolBar() {
    auto* bar = addToolBar(QStringLiteral("Principal"));
    bar->setObjectName(QStringLiteral("toolbar"));
    bar->setMovable(false);

    // Ferramentas do grafo: exclusao mutua por checked.
    auto* select = new QToolButton();
    select->setText(QStringLiteral("Selecionar"));
    select->setCheckable(true);
    select->setChecked(true);
    select->setShortcut(QKeySequence(QStringLiteral("V")));
    auto* hand = new QToolButton();
    hand->setText(QStringLiteral("Mao"));
    hand->setCheckable(true);
    hand->setShortcut(QKeySequence(QStringLiteral("H")));
    connect(select, &QToolButton::clicked, this, [this, hand] {
        m_nodes->setTool(NodeEditorWidget::Tool::Select);
        hand->setChecked(false);
    });
    connect(hand, &QToolButton::clicked, this, [this, select] {
        m_nodes->setTool(NodeEditorWidget::Tool::Hand);
        select->setChecked(false);
    });
    bar->addWidget(select);
    bar->addWidget(hand);
    bar->addSeparator();

    m_compositionList = new QComboBox();
    m_compositionList->setMinimumWidth(200);
    m_compositionList->setToolTip(QStringLiteral("Composicao em edicao"));
    bar->addWidget(m_compositionList);
    connect(m_compositionList, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                if (m_loading || index < 0) return;
                const CompId id = m_compositionList->itemData(index).toUInt();
                m_nodes->setComposition(id);
                m_viewer->setViewedComposition(id);
                refreshAll();
            });
    bar->addSeparator();

    auto* undoButton = new QToolButton();
    undoButton->setText(QStringLiteral("Desfazer"));
    undoButton->setToolTip(QStringLiteral("Ctrl+Z"));
    connect(undoButton, &QToolButton::clicked, this, &MainWindow::onUndo);
    bar->addWidget(undoButton);

    auto* redoButton = new QToolButton();
    redoButton->setText(QStringLiteral("Refazer"));
    connect(redoButton, &QToolButton::clicked, this, &MainWindow::onRedo);
    bar->addWidget(redoButton);
    bar->addSeparator();

    // Qualidade como slider de 3 posicoes: e um ajuste que se faz durante a
    // edicao, nao uma preferencia escondida em menu.
    bar->addWidget(new QLabel(QStringLiteral("Qualidade:"), bar));
    m_qualitySlider = new QSlider(Qt::Horizontal, bar);
    m_qualitySlider->setRange(0, 2);
    m_qualitySlider->setValue(1);
    m_qualitySlider->setMaximumWidth(100);
    m_qualitySlider->setToolTip(QStringLiteral("0 = mais leve, 2 = melhor"));
    bar->addWidget(m_qualitySlider);
    connect(m_qualitySlider, &QSlider::valueChanged, this,
            &MainWindow::onQualityChanged);

    m_lightButton = new QToolButton();
    m_lightButton->setText(QStringLiteral("Modo leve"));
    m_lightButton->setCheckable(true);
    m_lightButton->setToolTip(QStringLiteral("Ctrl+L - previa em 50%% e cache reduzido"));
    connect(m_lightButton, &QToolButton::clicked, this,
            [this] { setLightweightMode(m_lightButton->isChecked()); });
    bar->addWidget(m_lightButton);
}

void MainWindow::createTransportBar() {
    auto* bar = new QWidget();
    bar->setObjectName(QStringLiteral("toolbar"));
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(6, 3, 6, 3);
    layout->setSpacing(3);

    const auto button = [&](const QString& text, const QString& tip,
                            auto slot) {
        auto* b = new QToolButton(bar);
        b->setText(text);
        b->setToolTip(tip);
        connect(b, &QToolButton::clicked, this, slot);
        layout->addWidget(b);
        return b;
    };

    button(QStringLiteral("|◀"), QStringLiteral("Iniciar (Home)"),
           [this] { m_playback.goToStart(); });

    auto* play = new QToolButton(bar);
    play->setObjectName(QStringLiteral("playButton"));
    play->setText(QStringLiteral("▶"));
    play->setToolTip(QStringLiteral("Reproduzir / pausar (Espaco)"));
    connect(play, &QToolButton::clicked, this, [this] { m_playback.togglePlay(); });
    layout->addWidget(play);

    button(QStringLiteral("▶|"), QStringLiteral("Fim (End)"),
           [this] { m_playback.goToEnd(); });
    layout->addSpacing(8);

    button(QStringLiteral("◀|"), QStringLiteral("Ponto de edicao anterior"),
           [this] { m_playback.goToPreviousEditPoint(); });
    button(QStringLiteral("|▶"), QStringLiteral("Proximo ponto de edicao"),
           [this] { m_playback.goToNextEditPoint(); });
    layout->addSpacing(8);

    button(QStringLiteral("-1"), QStringLiteral("Um frame para tras (seta esquerda)"),
           [this] { m_playback.stepFrame(-1); });
    button(QStringLiteral("+1"), QStringLiteral("Um frame para frente (seta direita)"),
           [this] { m_playback.stepFrame(1); });
    layout->addSpacing(12);

    m_timecodeLabel = new QLabel(QStringLiteral("00:00:00:00"), bar);
    m_timecodeLabel->setFont(monoFont(11));
    m_timecodeLabel->setMinimumWidth(90);
    m_timecodeLabel->setToolTip(QStringLiteral("Tempo atual"));
    layout->addWidget(m_timecodeLabel);

    layout->addStretch(1);

    m_perfLabel = new QLabel(bar);
    m_perfLabel->setObjectName(QStringLiteral("hint"));
    m_perfLabel->setFont(monoFont(8));
    layout->addWidget(m_perfLabel);

    // A barra entra acima do visualizador, dentro do mesmo container, para
    // que o usuario veja tempo e imagem juntos.
    if (auto* holder = m_center->widget(0)) {
        if (auto* layout = qobject_cast<QVBoxLayout*>(holder->layout())) {
            layout->insertWidget(0, bar);
        }
    }

    m_statusRight = new QLabel();
    statusBar()->addPermanentWidget(m_statusRight);
    statusBar()->showMessage(QStringLiteral("Pronto"));
}

void MainWindow::connectSignals() {
    connect(&m_playback, &PlaybackController::timeChanged, this,
            &MainWindow::onTimeChanged);
    connect(&m_playback, &PlaybackController::stateChanged, this,
            [this](int) { updateStatusBar(); });
    connect(&m_playback, &PlaybackController::droppingFrames, this, [this](bool dropping) {
        if (dropping && !m_lightweight) {
            showStatus(QStringLiteral("Reproducao travando - ativando modo leve"));
            setLightweightMode(true);
        }
    });

    connect(m_nodes.get(), &NodeEditorWidget::selectionChanged, this,
            &MainWindow::onSelectionChanged);
    connect(m_nodes.get(), &NodeEditorWidget::graphChanged, this,
            &MainWindow::onGraphChanged);
    connect(m_nodes.get(), &NodeEditorWidget::statusMessage, this,
            [this](const QString& text) { showStatus(text); });

    // Mudancas estruturais viram comandos de desfazer.
    connect(m_nodes.get(), &NodeEditorWidget::nodeAdded, this, &MainWindow::onNodeAdded);
    connect(m_nodes.get(), &NodeEditorWidget::nodeRemoved, this,
            &MainWindow::onNodeRemoved);
    connect(m_nodes.get(), &NodeEditorWidget::connectionChanged, this,
            &MainWindow::onConnectionChanged);
    connect(m_nodes.get(), &NodeEditorWidget::nodeEnabledChanged, this,
            &MainWindow::onNodeEnabledChanged);

    connect(m_library.get(), &NodeLibraryWidget::nodeRequested, this,
            &MainWindow::onAddNodeRequested);

    connect(m_inspector.get(), &InspectorWidget::propertyChanged, this,
            &MainWindow::onPropertyChanged);
    connect(m_inspector.get(), &InspectorWidget::keyframeToggled, this,
            &MainWindow::onKeyframeToggled);
    connect(m_inspector.get(), &InspectorWidget::expressionToggled, this,
            &MainWindow::onExpressionToggled);
    connect(m_inspector.get(), &InspectorWidget::statusMessage, this,
            [this](const QString& text) { showStatus(text); });

    connect(m_timeline.get(), &TimelineWidget::timelineChanged, this,
            [this] { setDirty(true); refreshAll(); });
    connect(m_timeline.get(), &TimelineWidget::clipDoubleClicked, this,
            &MainWindow::onClipDoubleClicked);

    connect(m_viewer.get(), &ViewerWidget::colorPicked, this, [this](QColor c) {
        showStatus(QStringLiteral("Cor: %1").arg(c.name()));
    });
    connect(m_viewer.get(), &ViewerWidget::renderError, this,
            [this](const QString& message) { showStatus(message, 8000); });

    connect(m_undo.get(), &QUndoStack::indexChanged, this, [this](int) { setDirty(true); });
    connect(m_undo.get(), &QUndoStack::canUndoChanged, this,
            [this](bool can) { m_nodes->setUndoAvailable(can, m_undo->canRedo()); });
    connect(m_undo.get(), &QUndoStack::canRedoChanged, this,
            [this](bool can) { m_nodes->setUndoAvailable(m_undo->canUndo(), can); });
}

void MainWindow::refreshCompositionList() {
    if (!m_compositionList) return;
    const CompId current = m_nodes ? m_nodes->composition() : 0;

    m_loading = true;
    m_compositionList->clear();
    for (const auto& [id, comp] : m_project.compositions()) {
        m_compositionList->addItem(QString::fromStdString(comp.name()),
                                   static_cast<uint32_t>(id));
    }
    const int index = m_compositionList->findData(static_cast<uint32_t>(current));
    if (index >= 0) m_compositionList->setCurrentIndex(index);
    m_loading = false;
}

void MainWindow::refreshUndoState() {
    m_nodes->setUndoAvailable(m_undo->canUndo(), m_undo->canRedo());
    m_inspector->setUndoAvailable(m_undo->canUndo(), m_undo->canRedo());
}

void MainWindow::refreshAll() {
    if (m_viewer) {
        m_viewer->invalidate();
        m_viewer->clearCache();
    }
    if (m_timeline) m_timeline->update();
    m_inspector->update();
    updateStatusBar();
}

void MainWindow::updateStatusBar() {
    if (m_timecodeLabel) {
        m_timecodeLabel->setText(
            formatTimecode(m_playback.currentTime(), m_playback.fps()));
    }
    if (m_perfLabel && m_viewer) {
        m_perfLabel->setText(m_viewer->lastRenderStats());
    }
    if (m_statusRight) {
        size_t nodes = 0;
        if (m_nodes) {
            if (const Composition* c = m_project.composition(m_nodes->composition())) {
                nodes = c->graph().size();
            }
        }
        m_statusRight->setText(QStringLiteral("%1 nos   |   %2 clips   |   %3  ")
                                   .arg(nodes)
                                   .arg(m_project.timeline().contentRange().duration() > 0
                                            ? QStringLiteral("%.1f s").arg(
                                                  m_project.timeline().contentRange().duration())
                                            : QStringLiteral("vazia"))
                                   .arg(m_viewer ? m_viewer->rendererName() : QString()));
    }
}

void MainWindow::showStatus(const QString& text, int timeoutMs) {
    if (timeoutMs > 0) {
        statusBar()->showMessage(text, timeoutMs);
    } else {
        statusBar()->showMessage(text);
    }
}

void MainWindow::setWindowTitleFromProject() {
    const QString name = QString::fromStdString(m_project.name());
    setWindowTitle(m_dirty ? QStringLiteral("*%1 - Lumina").arg(name)
                           : QStringLiteral("%1 - Lumina").arg(name));
}

void MainWindow::setDirty(bool dirty) {
    if (m_dirty == dirty) return;
    m_dirty = dirty;
    setWindowTitleFromProject();
}

void MainWindow::pushCommand(QUndoCommand* command) {
    if (!command) return;
    // Nao registramos durante o carregamento: o desfazer teria que reverter um
    // projeto que o usuario nem chegou a ver.
    if (m_loading) {
        delete command;
        return;
    }
    m_undo->push(command);
}

void MainWindow::commit(QUndoCommand* command) {
    pushCommand(command);
    refreshAll();
    setDirty(true);
}

// ---------------------------------------------------------------------------
// Arquivo
// ---------------------------------------------------------------------------

void MainWindow::onNewProject() {
    if (m_dirty && !saveProject()) return;

    m_undo->clear();
    m_project.clear();
    m_project.createDefaultComposition();
    m_project.timeline().addStack("V1");
    m_project.timeline().addStack("V2");
    m_project.timeline().addStack("A1");

    nodes::ImageCache::instance().clear();
    if (m_sources) m_sources->invalidateAll();

    refreshCompositionList();
    m_nodes->setComposition(m_project.compositions().begin()->first);
    m_viewer->setViewedComposition(m_project.compositions().begin()->first);
    refreshAll();
    setDirty(false);
    showStatus(QStringLiteral("Projeto novo"));
}

void MainWindow::onOpen() {
    if (m_dirty && !saveProject()) return;

    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Abrir projeto"),
        io::ProjectSerializer::exportDirectoryFor(
            QString::fromStdString(m_project.filePath())),
        io::ProjectSerializer::fileDialogFilter());
    if (path.isEmpty()) return;
    openProject(path);
}

bool MainWindow::openProject(const QString& path) {
    const io::LoadResult result =
        io::ProjectSerializer::load(path.toStdString(), m_project);
    if (!result.ok) {
        QMessageBox::critical(this, QStringLiteral("Nao foi possivel abrir"),
                              QString::fromStdString(result.error));
        return false;
    }

    m_project.setFilePath(path.toStdString());
    m_project.setName(QFileInfo(path).completeBaseName().toStdString());
    m_undo->clear();
    nodes::ImageCache::instance().clear();
    if (m_sources) m_sources->invalidateAll();

    if (m_project.compositions().empty()) {
        m_project.createDefaultComposition();
    }
    if (m_project.timeline().stacks().empty()) {
        m_project.timeline().addStack("V1");
        m_project.timeline().addStack("A1");
    }

    const CompId first = m_project.compositions().begin()->first;
    refreshCompositionList();
    m_nodes->setComposition(first);
    m_viewer->setViewedComposition(first);
    refreshAll();
    setDirty(false);

    if (result.missingReferences > 0) {
        showStatus(QStringLiteral("%1 conexoes orfas: o arquivo foi salvo por outra "
                                  "versao do editor")
                       .arg(result.missingReferences),
                   8000);
    } else {
        showStatus(QStringLiteral("Aberto em %1 ms")
                       .arg(result.milliseconds, 0, 'f', 0));
    }
    return true;
}

bool MainWindow::saveProject() {
    if (m_project.filePath().empty()) return saveProjectAs();

    const auto result = io::ProjectSerializer::save(m_project, m_project.filePath());
    if (!result.ok) {
        QMessageBox::critical(this, QStringLiteral("Nao foi possivel salvar"),
                              QString::fromStdString(result.error));
        return false;
    }
    setDirty(false);
    showStatus(QStringLiteral("Salvo: %1 KB em %2 ms")
                   .arg(result.bytes / 1024)
                   .arg(result.milliseconds, 0, 'f', 0));
    return true;
}

bool MainWindow::saveProjectAs() {
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Salvar projeto"),
        io::ProjectSerializer::autosavePathFor(
            QString::fromStdString(m_project.filePath())),
        io::ProjectSerializer::fileDialogFilter());
    if (path.isEmpty()) return false;

    if (!path.endsWith(QLatin1Char('.') + io::ProjectSerializer::kExtension)) {
        path += QLatin1Char('.') + io::ProjectSerializer::kExtension;
    }
    m_project.setFilePath(path.toStdString());
    m_project.setName(QFileInfo(path).completeBaseName().toStdString());
    return saveProject();
}

// ---------------------------------------------------------------------------
// Grafo e selecao
// ---------------------------------------------------------------------------

QPointF MainWindow::spawnPositionForNewNode() const {
    const Composition* comp = m_project.composition(m_nodes->composition());
    if (!comp) return {0.0, 0.0};

    // Ao lado do no selecionado, se houver um. Sem selecao, a posicao do
    // primeiro no mais um deslocamento - o novo no nasce onde o olho esta.
    const auto& selection = m_nodes->selection();
    if (!selection.empty()) {
        if (const Node* n = comp->graph().node(selection.front())) {
            return {n->graphPos().x + 190.0, n->graphPos().y};
        }
    }
    if (NodeId first = comp->graph().output(); first != kInvalidId) {
        if (const Node* n = comp->graph().node(first)) {
            return {n->graphPos().x + 190.0, n->graphPos().y};
        }
    }
    return {0.0, 0.0};
}

void MainWindow::onAddNodeRequested(const std::string& type) {
    if (m_bottomTabs) m_bottomTabs->setCurrentWidget(m_nodes.get());
    // NodeEditorWidget::addNode emite nodeAdded com a copia do no, e o slot
    // abaixo transforma isso em comando de desfazer. Aqui so pedimos.
    m_nodes->addNode(type, spawnPositionForNewNode());
}

void MainWindow::onNodeAdded(CompId comp, Node node) {
    // O no ja esta no grafo: push() chama redo(), que tenta adicionar de novo.
    // addNode devolve nullptr para um id repetido, entao o estado fica certo -
    // mas e fragile. Em vez de depender disso, o comando recai no estado ja
    // aplicado.
    m_undo->push(new AddNodeCommand(m_project, comp, std::move(node), 0, kInvalidId));
    m_nodes->selectNode(node.id());
    m_inspector->setTarget(comp, node.id());
    setDirty(true);
}

void MainWindow::onNodeRemoved(CompId comp, Node node) {
    // A remocao ja aconteceu; o comando recai nela e devolve o no no undo.
    m_undo->push(new RemoveNodeCommand(m_project, comp, std::move(node), 0, kInvalidId));
    setDirty(true);
}

void MainWindow::onConnectionChanged(CompId comp, NodeId from, int fromOutput, NodeId to,
                                    int toInput, NodeId previousSource) {
    m_undo->push(new ConnectCommand(m_project, comp, from, fromOutput, to, toInput,
                                    previousSource,
                                    previousSource == kInvalidId ? QStringLiteral("conectar")
                                                                 : QStringLiteral("reconectar")));
    setDirty(true);
}

void MainWindow::onNodeEnabledChanged(CompId comp, NodeId node, bool was) {
    m_undo->push(new ToggleNodeCommand(m_project, comp, node, was,
                                       QStringLiteral("ligar/desligar no")));
    setDirty(true);
}

void MainWindow::onSelectionChanged() {
    syncSelectionToInspector();
}

void MainWindow::syncSelectionToInspector() {
    if (m_nodes->selection().size() == 1) {
        const NodeId id = m_nodes->selection().front();
        m_inspector->setTarget(m_nodes->composition(), id);

        const Composition* comp = m_project.composition(m_nodes->composition());
        if (const Node* n = comp ? comp->graph().node(id) : nullptr) {
            m_library->highlightType(n->type());
        }
    } else {
        m_inspector->clear();
    }
}

void MainWindow::onTimeChanged(double time) {
    Q_UNUSED(time);
    if (m_viewer) m_viewer->invalidate();
    if (m_inspector) m_inspector->update();
    if (m_timecodeLabel) {
        m_timecodeLabel->setText(
            formatTimecode(m_playback.currentTime(), m_playback.fps()));
    }
}

void MainWindow::onGraphChanged() {
    if (m_viewer) {
        m_viewer->invalidate();
        m_viewer->clearCache();
    }
    setDirty(true);
    updateStatusBar();
}

void MainWindow::onPropertyChanged(NodeId node, const QString& property, Value before,
                                   Value after) {
    // O comando carrega os dois valores, entao desfazer e refazer sao
    // simetricos e nao dependem de o controle ainda mostrar algo.
    commit(new SetPropertyCommand(m_project, m_nodes->composition(), node,
                                  property.toStdString(), before, after,
                                  QStringLiteral("editar %1").arg(property)));
}

void MainWindow::onKeyframeToggled(NodeId node, const QString& property, double time,
                                   bool existed, Value value) {
    commit(new KeyframeCommand(m_project, m_nodes->composition(), node,
                               property.toStdString(), time, existed, value,
                               existed ? QStringLiteral("remover keyframe")
                                       : QStringLiteral("criar keyframe")));
    if (m_timeline) m_timeline->update();
}

void MainWindow::onExpressionToggled(NodeId node, const QString& property) {
    Q_UNUSED(node);
    if (m_viewer) m_viewer->invalidate();
    setDirty(true);
    showStatus(QStringLiteral("Expressao de %1 alternada").arg(property));
}

void MainWindow::onClipDoubleClicked(ClipId id) {
    Clip* clip = m_project.timeline().findClip(id);
    if (!clip) return;
    m_nodes->setComposition(clip->comp);
    m_viewer->setViewedComposition(clip->comp);
    m_compositionList->setCurrentIndex(
        m_compositionList->findData(static_cast<uint32_t>(clip->comp)));
    m_bottomTabs->setCurrentWidget(m_nodes.get());
    refreshAll();
}

// ---------------------------------------------------------------------------
// Desfazer
// ---------------------------------------------------------------------------

void MainWindow::onUndo() {
    m_undo->undo();
    refreshAll();
    setDirty(true);
}

void MainWindow::onRedo() {
    m_undo->redo();
    refreshAll();
    setDirty(true);
}

// ---------------------------------------------------------------------------
// Desempenho
// ---------------------------------------------------------------------------

void MainWindow::onToggleLightweight() {
    setLightweightMode(!m_lightweight);
}

void MainWindow::setLightweightMode(bool on) {
    if (m_lightweight == on) return;
    m_lightweight = on;

    if (m_lightButton && m_lightButton->isChecked() != on) {
        m_lightButton->setChecked(on);
    }
    if (m_qualitySlider && m_qualitySlider->value() != (on ? 0 : 1)) {
        // Bloqueia o sinal: onQualityChanged reentra aqui e inverteria o modo.
        const QSignalBlocker blocker(m_qualitySlider);
        m_qualitySlider->setValue(on ? 0 : 1);
    }

    if (on) {
        // Meia resolucao, cache pequeno e proxy. Sao as tres medidas que mais
        // custam fragmento e que nao mudam o que o usuario esta editando.
        m_viewer->setPreviewScale(0.5);
        m_playback.setFrameCacheMemory(24ull * 1024 * 1024);
        m_playback.setQuality(0);
        nodes::ImageCache::instance().setMemoryBudget(64ull * 1024 * 1024);
        if (m_sources) m_sources->setUseProxy(true);
        showStatus(QStringLiteral("Modo leve: previa em 50%, cache e proxy ativos"));
    } else {
        m_viewer->setPreviewScale(1.0);
        m_playback.setFrameCacheMemory(96ull * 1024 * 1024);
        m_playback.setQuality(1);
        nodes::ImageCache::instance().setMemoryBudget(512ull * 1024 * 1024);
        if (m_sources) m_sources->setUseProxy(false);
        showStatus(QStringLiteral("Modo normal"));
    }
    refreshAll();
}

void MainWindow::setQuality(int level) {
    if (m_qualitySlider) {
        const QSignalBlocker blocker(m_qualitySlider);
        m_qualitySlider->setValue(std::clamp(level, 0, 2));
    }
    onQualityChanged(std::clamp(level, 0, 2));
}

void MainWindow::onQualityChanged(int level) {
    // Subir a qualidade enquanto o modo leve esta ligado desliga o modo leve:
    // e a intencao do usuario, nao um conflito de estado.
    if (m_lightweight && level > 0) {
        setLightweightMode(false);
        return;
    }

    m_playback.setQuality(level);
    switch (level) {
        case 0:
            m_viewer->setPreviewScale(0.5);
            m_playback.setFrameCacheMemory(24ull * 1024 * 1024);
            break;
        case 1:
            m_viewer->setPreviewScale(1.0);
            m_playback.setFrameCacheMemory(96ull * 1024 * 1024);
            break;
        default:
            m_viewer->setPreviewScale(1.0);
            m_playback.setFrameCacheMemory(192ull * 1024 * 1024);
            break;
    }
    refreshAll();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Um editor que perde trabalho nao e "leve", e um programa que nao pode
    // ser salvo. A confirmacao de saida e obrigatoria.
    if (!m_dirty) {
        event->accept();
        return;
    }

    const auto answer = QMessageBox::question(
        this, QStringLiteral("Sair"),
        QStringLiteral("O projeto tem alteracoes nao salvas.\n\nSalvar antes de sair?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    switch (answer) {
        case QMessageBox::Save:
            event->accept(saveProject() ? QEvent::Accept : QEvent::Ignore);
            break;
        case QMessageBox::Discard:
            event->accept();
            break;
        default:
            event->ignore();
            break;
    }
}

// ---------------------------------------------------------------------------
// Exportacao
// ---------------------------------------------------------------------------

void MainWindow::onExportFrame() {
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Exportar quadro"),
        io::ProjectSerializer::exportDirectoryFor(
            QString::fromStdString(m_project.filePath())),
        QStringLiteral("PNG (*.png)"));
    if (path.isEmpty()) return;

    const auto result = media::FrameExporter::exportFrame(
        m_project, m_nodes->composition(), m_playback.currentTime(), path.toStdString());
    if (!result.ok) {
        QMessageBox::warning(this, QStringLiteral("Exportar"),
                             QString::fromStdString(result.error));
    } else {
        showStatus(QStringLiteral("Quadro exportado em %1 ms")
                       .arg(result.milliseconds, 0, 'f', 0));
    }
}

void MainWindow::onExportImage() {
    onExportFrame();
}

void MainWindow::onExportMovie() {
    media::ExportDialog dialog(m_project, m_playback, this);
    if (dialog.exec() != QDialog::Accepted) return;
    showStatus(QStringLiteral("Exportacao concluida: %1").arg(dialog.resultSummary()));
}

}  // namespace lmn::ui
