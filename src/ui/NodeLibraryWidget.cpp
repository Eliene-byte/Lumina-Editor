#include "NodeLibraryWidget.h"

#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "core/NodeRegistry.h"
#include "ui/Theme.h"

namespace lmn::ui {
namespace {

// O nome do item e o tipo do no; o texto visivel e o nome de exibicao. Assim o
// item continua util como chave de busca depois de qualquer reordenacao.
constexpr int kTypeRole = Qt::UserRole + 1;
constexpr int kNodeIdRole = Qt::UserRole + 2;

}  // namespace

NodeLibraryWidget::NodeLibraryWidget(Project& project, PlaybackController& playback,
                                     QWidget* parent)
    : QWidget(parent), m_project(project), m_playback(playback) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("Buscar nos... (ex.: desfoque, cor, key)"));
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setUniformRowHeights(true);   // importante: a lista pode ter
                                          // centenas de itens
    m_tree->setIndentation(12);
    m_tree->setAlternatingRowColors(false);
    m_tree->setExpandsOnDoubleClick(false);
    layout->addWidget(m_tree, 1);

    connect(m_search, &QLineEdit::textChanged, this, &NodeLibraryWidget::setFilter);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem* item, int) { addNodeFromItem(item); });
    connect(m_tree, &QTreeWidget::itemActivated, this,
            [this](QTreeWidgetItem* item, int) { addNodeFromItem(item); });

    buildTree();
}

NodeLibraryWidget::~NodeLibraryWidget() = default;

void NodeLibraryWidget::buildTree() {
    m_items.clear();
    m_tree->clear();

    const auto& registry = NodeRegistry::instance();
    for (const std::string& category : registry.categories()) {
        auto* categoryItem = new QTreeWidgetItem(m_tree);
        categoryItem->setText(0, QString::fromStdString(category));
        categoryItem->setFirstColumnSpanned(true);
        categoryItem->setFlags(Qt::ItemIsEnabled);
        QFont f = uiFont(0, true);
        f.setPointSizeF(f.pointSizeF() * 0.92);
        categoryItem->setFont(0, f);
        categoryItem->setForeground(0, QColor(0x8b, 0x91, 0x9c));
        categoryItem->setExpanded(true);

        for (const NodeTypeDesc* desc : registry.typesInCategory(category)) {
            auto* item = new QTreeWidgetItem(categoryItem);
            item->setText(0, QString::fromStdString(desc->displayName));
            item->setData(0, kTypeRole, QString::fromStdString(desc->type));
            item->setToolTip(0, desc->description.empty()
                                     ? QString::fromStdString(desc->displayName)
                                     : QString::fromStdString(desc->description));
            m_items[desc->type] = item;
        }
    }
    m_tree->expandAll();
}

bool NodeLibraryWidget::matchesQuery(const NodeTypeDesc& desc, const QString& query) {
    if (query.isEmpty()) return true;

    if (desc.displayName.contains(query, Qt::CaseInsensitive)) return true;
    if (desc.type.contains(query.toStdString(), std::string::npos)) return true;
    if (desc.category.contains(query.toStdString(), std::string::npos)) return true;
    if (desc.description.contains(query, Qt::CaseInsensitive)) return true;

    for (const auto& spec : desc.properties) {
        if (spec.name.find(query.toStdString(), std::string::npos) !=
            std::string::npos) {
            return true;
        }
        if (!spec.tooltip.empty() &&
            QString::fromStdString(spec.tooltip).contains(query, Qt::CaseInsensitive)) {
            return true;
        }
        if (!spec.group.empty() &&
            QString::fromStdString(spec.group).contains(query, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

void NodeLibraryWidget::setFilter(const QString& text) {
    applyFilter(text.trimmed());
}

QString NodeLibraryWidget::filter() const {
    return m_search->text().trimmed();
}

void NodeLibraryWidget::applyFilter(const QString& text) {
    const auto& registry = NodeRegistry::instance();

    // Agrupa os resultados por categoria, na ordem do registro, em vez de
    // mostrar uma lista solta: com a biblioteca inteira, categoria e o que
    // torna a lista navegavel.
    m_tree->clear();
    m_items.clear();

    for (const std::string& category : registry.categories()) {
        std::vector<const NodeTypeDesc*> matching;
        for (const NodeTypeDesc* desc : registry.typesInCategory(category)) {
            if (matchesQuery(*desc, text)) matching.push_back(desc);
        }
        if (matching.empty()) continue;

        auto* categoryItem = new QTreeWidgetItem(m_tree);
        categoryItem->setText(0, QString::fromStdString(category));
        categoryItem->setFirstColumnSpanned(true);
        categoryItem->setFlags(Qt::ItemIsEnabled);
        QFont f = uiFont(0, true);
        f.setPointSizeF(f.pointSizeF() * 0.92);
        categoryItem->setFont(0, f);
        categoryItem->setForeground(0, QColor(0x8b, 0x91, 0x9c));
        categoryItem->setExpanded(true);

        for (const NodeTypeDesc* desc : matching) {
            auto* item = new QTreeWidgetItem(categoryItem);
            item->setText(0, QString::fromStdString(desc->displayName));
            item->setData(0, kTypeRole, QString::fromStdString(desc->type));
            item->setToolTip(0, desc->description.empty()
                                     ? QString::fromStdString(desc->displayName)
                                     : QString::fromStdString(desc->description));
            m_items[desc->type] = item;
        }
    }
    m_tree->expandAll();

    if (!text.isEmpty() && visibleTypeCount() == 0) {
        auto* empty = new QTreeWidgetItem(m_tree);
        empty->setText(0, QStringLiteral("nenhum no encontrado para \"%1\"").arg(text));
        empty->setFlags(Qt::ItemIsEnabled);
        empty->setForeground(0, QColor(0x6d, 0x73, 0x7e));
    }
}

void NodeLibraryWidget::addNodeFromItem(QTreeWidgetItem* item) {
    if (!item) return;
    const QVariant typeData = item->data(0, kTypeRole);
    if (!typeData.isValid()) return;   // clicou numa categoria
    emit nodeRequested(typeData.toString().toStdString());
}

QTreeWidgetItem* NodeLibraryWidget::itemForType(const std::string& type) const {
    const auto it = m_items.find(type);
    return it == m_items.end() ? nullptr : it->second;
}

void NodeLibraryWidget::highlightType(const std::string& type) {
    if (QTreeWidgetItem* item = itemForType(type)) {
        m_tree->setCurrentItem(item);
        m_tree->scrollToItem(item);
    }
}

int NodeLibraryWidget::visibleTypeCount() const {
    return static_cast<int>(m_items.size());
}

void NodeLibraryWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape && !m_search->text().isEmpty()) {
        m_search->clear();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

}  // namespace lmn::ui
