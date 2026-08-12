// ui/ProjectTreeView.cpp
// Renders the left-nav project tree from the Qt-independent ProjectTreeNode
// hierarchy. Each node carries its type + id in the item data so the MainWindow
// can route a selection to the detail panel.

#include "ui/ProjectTreeView.h"

#include <functional>

#include <QStandardItemModel>

namespace lodestar::ui {

ProjectTreeView::ProjectTreeView(QWidget* parent) : QTreeView(parent) {
    model_ = new QStandardItemModel(this);
    model_->setHorizontalHeaderLabels({"Project"});
    QTreeView::setModel(model_);
    setHeaderHidden(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);

    connect(this, &QTreeView::clicked, this, [this](const QModelIndex& index) {
        if (!index.isValid()) return;
        const auto* item = model_->itemFromIndex(index);
        if (!item) return;
        const QString type = item->data(Qt::UserRole + 1).toString();
        const QString id = item->data(Qt::UserRole + 2).toString();
        if (!id.isEmpty()) emit entitySelected(type, id);
    });
}

void ProjectTreeView::setModel(const std::vector<tracelink::ProjectTreeNode>& roots) {
    model_->clear();
    model_->setHorizontalHeaderLabels({"Project"});

    std::function<void(QStandardItem*, const tracelink::ProjectTreeNode&)> addNode =
        [&](QStandardItem* parent, const tracelink::ProjectTreeNode& node) {
            auto* item = new QStandardItem(QString::fromStdString(node.externalId));
            item->setData(QString::fromStdString(node.type), Qt::UserRole + 1);
            item->setData(QString::fromStdString(node.id), Qt::UserRole + 2);
            item->setEditable(false);
            if (parent) {
                parent->appendRow(item);
            } else {
                model_->appendRow(item);
            }
            for (const auto& child : node.children) {
                addNode(item, child);
            }
        };

    for (const auto& root : roots) {
        addNode(nullptr, root);
    }
    expandAll();
}

}  // namespace lodestar::ui
