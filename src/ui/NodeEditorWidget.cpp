#include "NodeEditorWidget.h"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "core/NodeRegistry.h"

namespace lmn::ui {
namespace {

// Cabecalho colorido por categoria: o usuario identifica "esta e uma correcao
// de cor" sem ler o nome.
QColor headerColorFor(const std::string& type, const Palette& pal) {
    const NodeTypeDesc* desc = NodeRegistry::instance().desc(type);
    const std::string cat = desc ? desc->category : std::string();

    if (cat == "Fonte")      return QColor(0x3a, 0x6b, 0x4a);
    if (cat == "Cor")        return QColor(0x6b, 0x4a, 0x7a);
    if (cat == "Filtro")     return QColor(0x4a, 0x5f, 0x7a);
    if (cat == "Mesclagem")  return QColor(0x7a, 0x5c, 0x36);
    if (cat == "Tempo")      return QColor(0x6b, 0x36, 0x36);
    if (cat == "Audio")      return QColor(0x36, 0x6b, 0x5a);
    return pal.nodeHeader;
}

QString formatValue(const Value& v) {
    switch (v.type()) {
        case ValueType::Bool:   return v.asBool() ? "sim" : "nao";
        case ValueType::Double: return QString::number(v.asDouble(), 'g', 4);
        case ValueType::String: return QString::fromStdString(v.asString());
        case ValueType::Color: {
            const Color c = v.asColor();
            return QStringLiteral("%1 %2 %3")
                .arg(c.r, 0, 'f', 2).arg(c.g, 0, 'f', 2).arg(c.b, 0, 'f', 2);
        }
        case ValueType::Vec2:
            return QStringLiteral("%1, %2")
                .arg(v.asVec2().x, 0, 'f', 1).arg(v.asVec2().y, 0, 'f', 1);
        case ValueType::Vec3: {
            const Vec3 p = v.asVec3();
            return QStringLiteral("%1, %2, %3")
                .arg(p.x, 0, 'f', 1).arg(p.y, 0, 'f', 1).arg(p.z, 0, 'f', 1);
        }
        case ValueType::Vec4: {
            const Vec4 p = v.asVec4();
            return QStringLiteral("%1, %2, %3, %4")
                .arg(p.x, 0, 'f', 1).arg(p.y, 0, 'f', 1)
                .arg(p.z, 0, 'f', 1).arg(p.w, 0, 'f', 1);
        }
        default: return {};
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

QPointF NodeEditorWidget::Geometry::inputPort(int index, int count) const {
    const double h = header.height();
    if (count <= 1) return QPointF(box.left(), box.top() + h + 8);
    const double step = 12.0;
    const double y = box.top() + h + 8 + index * step;
    return QPointF(box.left(), y);
}

NodeEditorWidget::NodeEditorWidget(Project& project, PlaybackController& playback,
                                   QWidget* parent)
    : QWidget(parent), m_project(project), m_playback(playback) {
    m_palette = Palette::dark();
    m_metrics = Metrics::scaled(1.0);

    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(false);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, m_palette.graphBackground);
    setPalette(pal);
    setContextMenuPolicy(Qt::DefaultContextMenu);

    m_pan = QPointF(120, 80);
}

NodeEditorWidget::~NodeEditorWidget() = default;

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------

QPointF NodeEditorWidget::graphToScreen(const QPointF& p) const {
    return QPointF(p.x * m_zoom + m_pan.x(), p.y * m_zoom + m_pan.y());
}

QPointF NodeEditorWidget::screenToGraph(const QPointF& p) const {
    return QPointF((p.x() - m_pan.x()) / m_zoom, (p.y() - m_pan.y()) / m_zoom);
}

void NodeEditorWidget::fitToGraph() {
    const Composition* comp = m_project.composition(m_comp);
    if (!comp || comp->graph().empty()) {
        m_zoom = 1.0;
        m_pan = QPointF(width() * 0.3, height() * 0.5);
        update();
        return;
    }

    QRectF bounds;
    bool first = true;
    for (NodeId id : comp->graph().nodeIds()) {
        const Node* n = comp->graph().node(id);
        if (!n) continue;
        const QRectF g = geometryFor(*n).box;
        if (first) { bounds = g; first = false; } else { bounds = bounds.united(g); }
    }
    if (first || bounds.isEmpty()) return;

    const QRectF available(width() - 40, height() - 40);
    m_zoom = std::clamp(std::min(available.width() / bounds.width(),
                                  available.height() / bounds.height()),
                        0.1, 2.0);
    m_pan = QPointF(20 - bounds.x() * m_zoom, 20 - bounds.y() * m_zoom);
    update();
}

void NodeEditorWidget::frameNode(NodeId id) {
    const Composition* comp = m_project.composition(m_comp);
    const Node* n = comp ? comp->graph().node(id) : nullptr;
    if (!n) return;

    const QRectF g = geometryFor(*n).box;
    m_zoom = std::clamp(std::min(width() / (g.width() * 2.2),
                                  height() / (g.height() * 3.0)),
                        0.1, 2.0);
    m_pan = QPointF(width() * 0.5 - (g.center().x()) * m_zoom,
                    height() * 0.5 - (g.center().y()) * m_zoom);
    update();
}

NodeEditorWidget::Geometry NodeEditorWidget::geometryFor(const Node& node) const {
    Geometry geo;
    const bool collapsed = node.isCollapsed();
    const int inputs = node.inputCount();
    const bool showPreview = m_showPreviews && !collapsed && node.type() != "audio.clip";

    int w = m_metrics.nodeMinWidth;
    int h = m_metrics.nodeHeaderHeight + 10;
    if (showPreview) h += 46;
    h += std::max(0, inputs) * 12;
    if (!node.properties().empty()) h += 14;
    if (collapsed) h = m_metrics.nodeHeaderHeight + 6;

    geo.box = QRectF(node.graphPos().x, node.graphPos().y, w, h);
    geo.header = QRectF(geo.box.left(), geo.box.top(), geo.box.width(),
                        m_metrics.nodeHeaderHeight);
    geo.outputPort = QPointF(geo.box.right(), geo.box.top() + m_metrics.nodeHeaderHeight / 2.0);
    geo.previewTopLeft = QPointF(geo.box.left() + 5, geo.header.bottom() + 4);
    geo.previewBottomRight = QPointF(geo.box.right() - 5, geo.header.bottom() + 48);

    // Converte para tela uma unica vez: assim os testes de clique e o desenho
    // usam exatamente as mesmas coordenadas.
    geo.box = QRectF(graphToScreen(geo.box.topLeft()),
                     graphToScreen(geo.box.bottomRight()));
    geo.header = QRectF(graphToScreen(geo.header.topLeft()),
                        graphToScreen(geo.header.bottomRight()));
    geo.outputPort = graphToScreen(geo.outputPort);
    geo.previewTopLeft = graphToScreen(geo.previewTopLeft);
    geo.previewBottomRight = graphToScreen(geo.previewBottomRight);
    return geo;
}

// ---------------------------------------------------------------------------
// Selecao
// ---------------------------------------------------------------------------

bool NodeEditorWidget::isSelected(NodeId id) const {
    return std::find(m_selection.begin(), m_selection.end(), id) != m_selection.end();
}

void NodeEditorWidget::selectNode(NodeId id, bool addToSelection) {
    if (id == kInvalidId) {
        clearSelection();
        return;
    }
    if (addToSelection) {
        if (isSelected(id)) {
            m_selection.erase(
                std::remove(m_selection.begin(), m_selection.end(), id),
                m_selection.end());
        } else {
            m_selection.push_back(id);
        }
    } else {
        m_selection.assign(1, id);
    }
    m_singleSelection = m_selection.size() == 1 ? m_selection.front() : kInvalidId;
    update();
    emit selectionChanged();
}

void NodeEditorWidget::clearSelection() {
    if (m_selection.empty()) return;
    m_selection.clear();
    m_singleSelection = kInvalidId;
    update();
    emit selectionChanged();
}

void NodeEditorWidget::selectAll() {
    const Composition* comp = m_project.composition(m_comp);
    if (!comp) return;
    m_selection = comp->graph().nodeIds();
    m_singleSelection = kInvalidId;
    update();
    emit selectionChanged();
}

Node* NodeEditorWidget::selectedSingleNode() {
    const Composition* comp = m_project.composition(m_comp);
    if (!comp || m_selection.size() != 1) return nullptr;
    return const_cast<Composition*>(comp)->graph().node(m_selection.front());
}

void NodeEditorWidget::selectInBox(const QRectF& box, bool additive) {
    const Composition* comp = m_project.composition(m_comp);
    if (!comp) return;
    if (!additive) m_selection.clear();
    for (NodeId id : comp->graph().nodeIds()) {
        const Node* n = comp->graph().node(id);
        if (!n) continue;
        if (geometryFor(*n).box.intersects(box) &&
            std::find(m_selection.begin(), m_selection.end(), id) == m_selection.end()) {
            m_selection.push_back(id);
        }
    }
    m_singleSelection = m_selection.size() == 1 ? m_selection.front() : kInvalidId;
}

// ---------------------------------------------------------------------------
// Hit test
// ---------------------------------------------------------------------------

NodeId NodeEditorWidget::nodeAt(const QPointF& screenPos) const {
    const Composition* comp = m_project.composition(m_comp);
    if (!comp) return kInvalidId;

    // Do ultimo para o primeiro: o ultimo criado e o que esta por cima.
    const auto ids = comp->graph().nodeIds();
    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
        const Node* n = comp->graph().node(*it);
        if (n && geometryFor(*n).box.contains(screenPos)) return *it;
    }
    return kInvalidId;
}

int NodeEditorWidget::portAt(const QPointF& screenPos, NodeId& node, bool& isInput,
                             int& index) const {
    const Composition* comp = m_project.composition(m_comp);
    if (!comp) return 0;

    constexpr double hitRadius = 7.0;
    for (NodeId id : comp->graph().nodeIds()) {
        const Node* n = comp->graph().node(id);
        if (!n) continue;
        const Geometry geo = geometryFor(*n);

        if (QLineF(geo.outputPort, screenPos).length() <= hitRadius) {
            node = id;
            isInput = false;
            index = 0;
            return 1;
        }
        for (int s = 0; s < n->inputCount(); ++s) {
            if (QLineF(geo.inputPort(s, n->inputCount()), screenPos).length() <=
                hitRadius) {
                node = id;
                isInput = true;
                index = s;
                return 1;
            }
        }
    }
    return 0;
}

void NodeEditorWidget::updateHover(const QPointF& pos) {
    const NodeId previous = m_hoverNode;
    m_hoverNode = nodeAt(pos);

    NodeId dummy = kInvalidId;
    bool isInput = false;
    int index = 0;
    if (portAt(pos, dummy, isInput, index)) {
        setCursor(isInput ? Qt::SizeHorCursor : Qt::CrossCursor);
    } else if (m_hoverNode != kInvalidId) {
        setCursor(Qt::OpenHandCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }

    if (previous != m_hoverNode) update();
}

// ---------------------------------------------------------------------------
// Pintura
// ---------------------------------------------------------------------------

void NodeEditorWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    p.fillRect(rect(), m_palette.graphBackground);
    paintGrid(p);
    paintWires(p);
    if (m_wiring) paintWirePreview(p);

    const Composition* comp = m_project.composition(m_comp);
    if (comp) {
        for (NodeId id : comp->graph().nodeIds()) {
            const Node* n = comp->graph().node(id);
            if (!n) continue;
            paintNode(p, *n, geometryFor(*n));
        }
    }

    if (m_dragMode == DragMode::BoxSelect) {
        p.setPen(QPen(m_palette.accent, 1, Qt::DashLine));
        p.setBrush(QColor(m_palette.accent.red(), m_palette.accent.green(),
                          m_palette.accent.blue(), 30));
        p.drawRect(m_selectionBox);
    }

    paintHeader(p);
}

void NodeEditorWidget::paintGrid(QPainter& p) {
    if (m_zoom < 0.35) return;   // abaixo disso a grade vira ruido

    const QPointF origin = graphToScreen({0, 0});
    const double step = m_metrics.gridStep * m_zoom;

    p.setPen(QPen(m_palette.graphGrid, 1));
    for (double x = std::fmod(origin.x(), step); x < width(); x += step) {
        p.drawLine(QPointF(x, 0), QPointF(x, height()));
    }
    for (double y = std::fmod(origin.y(), step); y < height(); y += step) {
        p.drawLine(QPointF(0, y), QPointF(width(), y));
    }
}

void NodeEditorWidget::paintWires(QPainter& p) {
    const Composition* comp = m_project.composition(m_comp);
    if (!comp) return;

    // Curva em S horizontal, como no Nuke e no Fusion: o oculo acompanha o
    // cabo e nao a ponta reta, o que ajuda a ler grafos com many nodes.
    for (const Connection& c : comp->graph().connections()) {
        const Node* from = comp->graph().node(c.from);
        const Node* to = comp->graph().node(c.to);
        if (!from || !to) continue;

        const Geometry gFrom = geometryFor(*from);
        const Geometry gTo = geometryFor(*to);
        const QPointF start = gFrom.outputPort;
        const QPointF end = gTo.inputPort(c.toInput, to->inputCount());

        const bool active = isSelected(c.from) || isSelected(c.to);
        p.setPen(QPen(active ? m_palette.graphWireActive : m_palette.graphWire,
                      active ? 2.0 : 1.4));

        QPainterPath path(start);
        const double dx = std::max(30.0, std::abs(end.x() - start.x) * 0.5);
        path.cubicTo(start + QPointF(dx, 0), end - QPointF(dx, 0), end);
        p.drawPath(path);
    }
}

void NodeEditorWidget::paintWirePreview(QPainter& p) {
    const Composition* comp = m_project.composition(m_comp);
    const Node* from = comp ? comp->graph().node(m_wireFrom) : nullptr;
    if (!from) return;

    const QPointF start = geometryFor(*from).outputPort;
    const QPointF end = m_wireCursor;

    p.setPen(QPen(m_palette.accent, 2.0, Qt::DashLine));
    QPainterPath path(start);
    const double dx = std::max(30.0, std::abs(end.x() - start.x) * 0.5);
    path.cubicTo(start + QPointF(dx, 0), end - QPointF(dx, 0), end);
    p.drawPath(path);
}

void NodeEditorWidget::paintNode(QPainter& p, const Node& node, const Geometry& geo) {
    const bool selected = isSelected(node.id());
    const bool hovered = m_hoverNode == node.id();
    const bool dimmed = !node.isEnabled();

    p.save();
    if (dimmed) p.setOpacity(0.45);

    // Sombra sutil: da profundidade sem custo relevante.
    if (hovered || selected) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 60));
        p.drawRoundedRect(geo.box.adjusted(2, 3, 2, 5), m_metrics.nodeCornerRadius,
                          m_metrics.nodeCornerRadius);
    }

    // Corpo
    p.setBrush(m_palette.surface);
    p.setPen(QPen(selected ? m_palette.accent : m_palette.border,
                  selected ? 2.0 : 1.0));
    p.drawRoundedRect(geo.box, m_metrics.nodeCornerRadius, m_metrics.nodeCornerRadius);

    // Cabecalho
    QPainterPath header;
    header.addRoundedRect(geo.header, m_metrics.nodeCornerRadius,
                          m_metrics.nodeCornerRadius);
    p.setPen(Qt::NoPen);
    p.setBrush(headerColorFor(node.type(), m_palette));
    p.drawPath(header);
    // O canto inferior do cabecalho e reto, para o corpo continuar reto.
    p.setPen(Qt::NoPen);
    p.setBrush(headerColorFor(node.type(), m_palette));
    p.drawRect(QRectF(geo.header.left(), geo.header.bottom() - 2,
                      geo.header.width(), 2));

    // Nome
    p.setPen(m_palette.textInverted);
    p.setFont(uiFont(0, true));
    const QRectF nameRect = geo.header.adjusted(6, 0, -6, 0);
    p.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
               QString::fromStdString(node.displayName()));

    // Marcadores: solo, cadeado, expressao com erro
    float right = geo.header.right() - 6;
    if (node.flag(NodeFlag::Solo)) {
        p.setBrush(QColor(0xe0, 0xb0, 0x50));
        p.drawEllipse(QPointF(right - 4, geo.header.center().y()), 3, 3);
        right -= 12;
    }
    if (node.flag(NodeFlag::Locked)) {
        p.setPen(QPen(m_palette.textInverted, 1.2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(right - 8, geo.header.center().y() - 3, 6, 6));
        right -= 14;
    }
    bool hasExpression = false;
    for (const auto& [name, prop] : node.properties()) {
        if (prop.hasExpression() && prop.expressionEnabled()) {
            hasExpression = true;
            break;
        }
    }
    if (hasExpression) {
        p.setPen(QPen(QColor(0x8a, 0xd0, 0x8a), 1.6));
        p.setBrush(Qt::NoBrush);
        // Pequeno "f(x)" de indicacao: um glifo, nao um texto, para nao
        // depender de fonte.
        p.drawText(QRectF(right - 12, geo.header.top(), 12, geo.header.height()),
                   Qt::AlignCenter, QStringLiteral("fx"));
    }

    // Area de previa
    if (!node.isCollapsed() && m_showPreviews &&
        node.type() != "audio.clip") {
        const QRectF preview = QRectF(geo.previewTopLeft, geo.previewBottomRight);
        p.setBrush(m_palette.surfaceAlt);
        p.setPen(QPen(m_palette.border, 1));
        p.drawRect(preview);

        // A previa real vem do thumbnail cache; enquanto nao houver, um
        // marcador neutro e melhor do que um retangulo colorido inventado.
        p.setPen(m_palette.textDim);
        p.setFont(uiFont(0));
        p.drawText(preview, Qt::AlignCenter, QStringLiteral("previa"));
    }

    // Portas
    if (!node.isCollapsed()) {
        p.setBrush(m_palette.surfaceAlt);
        p.setPen(QPen(m_palette.borderStrong, 1));

        for (int s = 0; s < node.inputCount(); ++s) {
            const QPointF port = geo.inputPort(s, node.inputCount());
            const bool connected = node.isConnected(s);
            p.setBrush(connected ? m_palette.accent : m_palette.surfaceAlt);
            p.drawEllipse(port, m_metrics.portRadius, m_metrics.portRadius);
        }

        p.setBrush(m_palette.accentDim);
        p.setPen(QPen(m_palette.borderStrong, 1));
        p.drawEllipse(geo.outputPort, m_metrics.portRadius, m_metrics.portRadius);
    }

    // Marcador de saida da composicao
    if (comp && comp->graph().output() == node.id()) {
        p.setPen(QPen(m_palette.success, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(geo.box.adjusted(-2, -2, 2, 2), m_metrics.nodeCornerRadius,
                          m_metrics.nodeCornerRadius);
    }

    p.restore();
}

void NodeEditorWidget::paintHeader(QPainter& p) {
    const Composition* comp = m_project.composition(m_comp);
    p.setFont(uiFont(0));
    p.setPen(m_palette.textDim);

    QString info;
    if (comp) {
        info = QStringLiteral("%1  -  %2 nos  -  %3 x %4  -  %5 fps")
                   .arg(QString::fromStdString(comp->name()))
                   .arg(comp->graph().size())
                   .arg(comp->size().width)
                   .arg(comp->size().height)
                   .arg(comp->fps(), 0, 'g', 3);
    } else {
        info = QStringLiteral("nenhuma composicao");
    }
    p.drawText(QRectF(10, 4, width() - 20, 18), Qt::AlignLeft | Qt::AlignVCenter, info);

    if (m_zoom < 1.0) {
        p.drawText(QRectF(width() - 90, 4, 80, 18), Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("%1%").arg(m_zoom * 100, 0, 'f', 0));
    }
}

// ---------------------------------------------------------------------------
// Interacao
// ---------------------------------------------------------------------------

void NodeEditorWidget::mousePressEvent(QMouseEvent* event) {
    setFocus();
    const QPointF pos = event->position();
    m_dragStartScreen = pos;
    m_dragStartGraph = screenToGraph(pos);
    m_dragLast = pos;

    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && m_tool == Tool::Hand)) {
        m_dragMode = DragMode::Pan;
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        // 1. Porta: comecar ou terminar uma conexao
        NodeId portNode = kInvalidId;
        bool isInput = false;
        int portIndex = 0;
        if (portAt(pos, portNode, isInput, portIndex)) {
            const Composition* comp = m_project.composition(m_comp);
            const Node* n = comp ? comp->graph().node(portNode) : nullptr;
            if (n) {
                if (m_wiring) {
                    // Solta sobre a entrada: conecta.
                    Composition* comp = m_project.composition(m_comp);
                    if (isInput && comp) {
                        // Guarda a fonte anterior ANTES de ligar: e o que o
                        // comando de desfazer precisa para religar o cabo.
                        const NodeId previous = n->input(portIndex);
                        if (comp->graph().connect(m_wireFrom, m_wireOutput, portNode,
                                                  portIndex)) {
                            emit connectionChanged(m_comp, m_wireFrom, m_wireOutput,
                                                  portNode, portIndex, previous);
                            emit statusMessage(QStringLiteral("Conectado"));
                            emit graphChanged();
                        } else {
                            emit statusMessage(
                                QStringLiteral("Conexao invalida: criaria um ciclo"));
                        }
                    }
                    cancelConnection();
                } else if (!isInput) {
                    // Arrastar da saida de uma entrada ja conectada puxa o cabo
                    // existente, como no Nuke: liga a ponta solta na nova fonte
                    // e deixa a entrada pronta para reconectar.
                    Composition* comp = m_project.composition(m_comp);
                    if (n->isConnected(0) && comp) {
                        const NodeId oldSource = n->input(0);
                        const int oldOutput = n->inputOutputIndex(0);
                        m_wireFrom = oldSource;
                        m_wireOutput = oldOutput;
                        comp->graph().disconnect(portNode, 0);
                        // A desconexao tambem e uma mudanca: sem este sinal,
                        // desfazer nao saberia que o cabo saiu.
                        emit connectionChanged(m_comp, oldSource, oldOutput, portNode, 0,
                                               oldSource);
                        m_wiring = true;
                        m_wireToInput = true;
                        m_wireCursor = pos;
                        m_dragMode = DragMode::Wire;
                        emit statusMessage(
                            QStringLiteral("Cabo desconectado: solte em outra entrada"));
                        emit graphChanged();
                        return;
                    }
                    m_wireFrom = portNode;
                    m_wireOutput = 0;
                    m_wiring = true;
                    m_wireToInput = true;
                    m_wireCursor = pos;
                    m_dragMode = DragMode::Wire;
                }
                update();
                return;
            }
        }

        // 2. No
        const NodeId hit = nodeAt(pos);
        if (hit != kInvalidId) {
            if (!isSelected(hit)) selectNode(hit, event->modifiers() & Qt::ShiftModifier);
            m_dragNode = hit;
            m_dragMode = DragMode::Nodes;
            return;
        }

        // 3. Fundo: caixa de selecao
        m_dragMode = DragMode::BoxSelect;
        m_selectionBox = QRectF(pos, pos);
        if (!(event->modifiers() & Qt::ShiftModifier)) clearSelection();
    }
}

void NodeEditorWidget::mouseMoveEvent(QMouseEvent* event) {
    const QPointF pos = event->position();
    m_wireCursor = pos;

    switch (m_dragMode) {
        case DragMode::Pan: {
            m_pan += pos - m_dragStartScreen;
            m_dragStartScreen = pos;
            update();
            return;
        }
        case DragMode::Nodes: {
            const QPointF delta = screenToGraph(pos) - m_dragStartGraph;
            moveSelection(delta);
            m_dragStartGraph = screenToGraph(pos);
            return;
        }
        case DragMode::Wire: {
            update();
            return;
        }
        case DragMode::BoxSelect: {
            m_selectionBox = QRectF(m_dragStartScreen, pos).normalized();
            update();
            return;
        }
        case DragMode::None:
        default:
            updateHover(pos);
            return;
    }
}

void NodeEditorWidget::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);

    switch (m_dragMode) {
        case DragMode::Nodes: {
            // Encaixe no final: e aqui que o no "gruda" na grade, e nao a
            // cada pixel do arraste.
            Composition* comp = m_project.composition(m_comp);
            if (comp && m_snap) {
                for (NodeId id : m_selection) {
                    if (Node* n = comp->graph().node(id)) {
                        n->setGraphPos(snapValue(n->graphPos()));
                    }
                }
            }
            emit graphChanged();
            update();
            break;
        }
        case DragMode::Wire:
            cancelConnection();
            break;
        case DragMode::BoxSelect:
            if (m_selectionBox.width() > 3 && m_selectionBox.height() > 3) {
                selectInBox(m_selectionBox, event->modifiers() & Qt::ShiftModifier);
                emit selectionChanged();
            }
            update();
            break;
        case DragMode::Pan:
            setCursor(Qt::OpenHandCursor);
            break;
        default:
            break;
    }
    m_dragMode = DragMode::None;
    m_dragNode = kInvalidId;
}

void NodeEditorWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    const NodeId hit = nodeAt(event->position());
    if (hit != kInvalidId) {
        m_editingNode = hit;
        emit editingChanged(hit);
        emit nodeDoubleClicked(hit);
    }
}

void NodeEditorWidget::wheelEvent(QWheelEvent* event) {
    const double factor = event->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12;

    // Zoom ancorado no cursor: e o que faz o trabalho sob o mouse continuar
    // onde esta.
    const QPointF before = screenToGraph(event->position());
    m_zoom = std::clamp(m_zoom * factor, 0.1, 4.0);
    const QPointF after = screenToGraph(event->position());
    m_pan += (after - before) * m_zoom;
    update();
    event->accept();
}

void NodeEditorWidget::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            deleteSelection();
            return;
        case Qt::Key_Escape:
            cancelConnection();
            clearSelection();
            return;
        case Qt::Key_F:
            frameNode(m_singleSelection != kInvalidId ? m_singleSelection
                                                     : m_selection.front());
            return;
        case Qt::Key_A:
            if (event->modifiers() & Qt::ControlModifier) {
                selectAll();
                return;
            }
            break;
        case Qt::Key_D:
            if (event->modifiers() & Qt::ControlModifier) {
                duplicateSelection();
                return;
            }
            break;
        case Qt::Key_L:
            toggleEnabled();
            return;
        case Qt::Key_S:
            toggleSolo();
            return;
        case Qt::Key_Space:
            m_playback.togglePlay();
            return;
        case Qt::Key_Left:
            m_playback.stepFrame(-1);
            return;
        case Qt::Key_Right:
            m_playback.stepFrame(1);
            return;
        case Qt::Key_Up:
            bringToFront();
            return;
        case Qt::Key_Down:
            sendToBack();
            return;
        default:
            break;
    }
    QWidget::keyPressEvent(event);
}

void NodeEditorWidget::contextMenuEvent(QContextMenuEvent* event) {
    const NodeId hit = nodeAt(event->pos());
    if (hit != kInvalidId && !isSelected(hit)) selectNode(hit);
    emit contextMenuRequested(hit, event->globalPos());
}

// ---------------------------------------------------------------------------
// Edicao
// ---------------------------------------------------------------------------

Node* NodeEditorWidget::addNode(const std::string& type, const QPointF& at) {
    Composition* comp = m_project.composition(m_comp);
    if (!comp) return nullptr;

    Node* node = comp->graph().createNode(type);
    if (!node) {
        emit statusMessage(QStringLiteral("Tipo de no desconhecido: %1")
                               .arg(QString::fromStdString(type)));
        return nullptr;
    }

    QPointF pos = at;
    if (m_snap) pos = snapValue(pos);
    node->setGraphPos(pos);
    node->setPassIndex(static_cast<int>(comp->graph().size()));

    // Anuncia com uma copia, porque o receptor cria um comando de desfazer que
    // precisa sobreviver a qualquer mudanca futura do no.
    emit nodeAdded(m_comp, *node);

    selectNode(node->id());
    emit graphChanged();
    update();
    return node;
}

void NodeEditorWidget::deleteSelection() {
    Composition* comp = m_project.composition(m_comp);
    if (!comp || m_selection.empty()) return;

    const std::vector<NodeId> doomed = m_selection;

    // Anuncia a remocao ANTES de remover, carregando uma copia do no. O
    // receptor monta o comando de desfazer com essa copia; depois da remocao
    // o original nao existe mais.
    for (NodeId id : doomed) {
        if (const Node* node = comp->graph().node(id)) {
            const NodeId from = node->input(0);
            emit nodeRemoved(m_comp, *node);
            Q_UNUSED(from);
        }
    }

    for (NodeId id : doomed) comp->graph().removeNode(id);
    clearSelection();
    emit graphChanged();
    emit statusMessage(QStringLiteral("%1 no(s) removido(s)").arg(doomed.size()));
}

void NodeEditorWidget::duplicateSelection() {
    Composition* comp = m_project.composition(m_comp);
    if (!comp || m_selection.empty()) return;

    std::vector<NodeId> newSelection;
    for (NodeId id : m_selection) {
        const Node* src = comp->graph().node(id);
        if (!src) continue;

        // O grafo escolhe o id livre; entao clonamos por cima do no vazio.
        Node* placeholder = comp->graph().createNode(src->type());
        if (!placeholder) continue;
        const NodeId fresh = placeholder->id();

        Node copy = src->cloned(fresh);
        copy.setGraphPos(snapValue(src->graphPos() + Vec2{24, 24}));
        copy.setName(src->name() + " copia");

        // addNode substitui: remove o placeholder e insere a copia.
        comp->graph().removeNode(fresh);
        Node* inserted = comp->graph().addNode(std::move(copy));
        if (inserted) newSelection.push_back(fresh);
    }

    if (!newSelection.empty()) {
        m_selection = newSelection;
        m_singleSelection = newSelection.size() == 1 ? newSelection.front() : kInvalidId;
        emit graphChanged();
        emit selectionChanged();
        update();
    }
}

void NodeEditorWidget::groupSelection() {
    // Agrupar nos como um unico objeto e a proxima etapa: por enquanto o
    // comando existe e informa o que falta, em vez de fingir que funciona.
    if (m_selection.size() < 2) {
        emit statusMessage(QStringLiteral("Selecione ao menos dois nos para agrupar"));
        return;
    }
    emit statusMessage(QStringLiteral("Agrupar ainda nao implementado"));
}

void NodeEditorWidget::ungroupSelection() {
    emit statusMessage(QStringLiteral("Desagrupar ainda nao implementado"));
}

void NodeEditorWidget::bringToFront() {
    Composition* comp = m_project.composition(m_comp);
    if (!comp) return;
    for (NodeId id : m_selection) {
        if (Node* n = comp->graph().node(id)) n->setPassIndex(n->passIndex() + 1000);
    }
    emit graphChanged();
    update();
}

void NodeEditorWidget::sendToBack() {
    Composition* comp = m_project.composition(m_comp);
    if (!comp) return;
    for (NodeId id : m_selection) {
        if (Node* n = comp->graph().node(id)) n->setPassIndex(n->passIndex() - 1000);
    }
    emit graphChanged();
    update();
}

void NodeEditorWidget::toggleEnabled() {
    Composition* comp = m_project.composition(m_comp);
    if (!comp) return;
    for (NodeId id : m_selection) {
        if (Node* n = comp->graph().node(id)) {
            // Anuncia o estado anterior: o comando de desfazer precisa dele para
            // religar o no, e nao apenas o contrario.
            const bool was = n->isEnabled();
            n->setEnabled(!was);
            emit nodeEnabledChanged(m_comp, id, was);
        }
    }
    emit graphChanged();
    update();
}

void NodeEditorWidget::toggleSolo() {
    Composition* comp = m_project.composition(m_comp);
    if (!comp) return;
    for (NodeId id : m_selection) {
        if (Node* n = comp->graph().node(id)) n->setFlag(NodeFlag::Solo, !n->flag(NodeFlag::Solo));
    }
    emit graphChanged();
    update();
}

void NodeEditorWidget::toggleLock() {
    Composition* comp = m_project.composition(m_comp);
    if (!comp) return;
    for (NodeId id : m_selection) {
        if (Node* n = comp->graph().node(id)) n->setFlag(NodeFlag::Locked, !n->flag(NodeFlag::Locked));
    }
    update();
}

void NodeEditorWidget::beginConnection(NodeId from, int outputIndex) {
    m_wireFrom = from;
    m_wireOutput = outputIndex;
    m_wiring = true;
    update();
}

void NodeEditorWidget::cancelConnection() {
    if (!m_wiring) return;
    m_wiring = false;
    m_wireFrom = kInvalidId;
    m_dragMode = DragMode::None;
    update();
}

void NodeEditorWidget::setComposition(CompId id) {
    m_comp = id;
    clearSelection();
    m_hoverNode = kInvalidId;
    fitToGraph();
    emit graphChanged();
}

void NodeEditorWidget::setUndoAvailable(bool canUndo, bool canRedo) {
    m_undoAvailable = canUndo;
    m_redoAvailable = canRedo;
    update();
}

void NodeEditorWidget::moveSelection(const QPointF& delta) {
    Composition* comp = m_project.composition(m_comp);
    if (!comp) return;
    for (NodeId id : m_selection) {
        if (Node* n = comp->graph().node(id)) {
            n->setGraphPos(n->graphPos() + delta);
        }
    }
    update();
}

bool NodeEditorWidget::onGrid(const QPointF& p) const {
    return m_snap;
}

QPointF NodeEditorWidget::snapValue(const QPointF& p) const {
    if (!m_snap) return p;
    const double g = m_metrics.gridStep;
    return QPointF(std::round(p.x / g) * g, std::round(p.y / g) * g);
}

void NodeEditorWidget::snapToGrid(QPointF& pos) const {
    pos = snapValue(pos);
}

}  // namespace lmn::ui
