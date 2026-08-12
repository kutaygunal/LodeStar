// ui/ImpactView.cpp
// Renders the affected-entity tree and blocked transitions for an impact run.

#include "ui/ImpactView.h"

#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace lodestar::ui {

ImpactView::ImpactView(QWidget* parent) : QWidget(parent) {
    tree_ = new QTreeWidget(this);
    tree_->setColumnCount(2);
    tree_->setHeaderLabels({"Entity", "Type"});

    transitions_ = new QListWidget(this);
    transitions_->setMaximumHeight(140);

    summary_ = new QLabel(this);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(summary_);
    layout->addWidget(tree_);
    layout->addWidget(new QLabel("Blocked transitions:", this));
    layout->addWidget(transitions_);
}

void ImpactView::setModel(const tracelink::ImpactViewModel& model) {
    tree_->clear();
    transitions_->clear();

    summary_->setText(QString::number(model.affected.size()) +
                      " affected entities, " +
                      QString::number(model.blockedTransitions.size()) +
                      " blocked transition(s)");

    // Add parents first so children can be attached by depth.
    std::vector<QTreeWidgetItem*> parents(model.affected.size(), nullptr);
    for (size_t i = 0; i < model.affected.size(); ++i) {
        const auto& node = model.affected[i];
        auto* item = new QTreeWidgetItem(
            {QString::fromStdString(node.externalId),
             QString::fromStdString(node.type)});
        const int depth = node.depth;
        if (depth == 0) {
            tree_->addTopLevelItem(item);
        } else {
            // Attach to the closest previous node at (depth-1).
            QTreeWidgetItem* parentItem = nullptr;
            for (size_t j = i; j-- > 0;) {
                if (model.affected[j].depth == depth - 1) {
                    parentItem = parents[j];
                    break;
                }
            }
            if (parentItem) parentItem->addChild(item);
            else tree_->addTopLevelItem(item);
        }
        parents[i] = item;
    }
    tree_->expandAll();

    for (const auto& t : model.blockedTransitions) {
        transitions_->addItem(QString::fromStdString(t));
    }
}

}  // namespace lodestar::ui
