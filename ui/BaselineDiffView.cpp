// ui/BaselineDiffView.cpp
// Renders the WP-9 baseline visual compare (one row per changed entity) and
// provides a per-item rollback button that restores a single entity to the
// older baseline via UiWiringService::rollbackEntity().

#include "ui/BaselineDiffView.h"

#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace lodestar::ui {

BaselineDiffView::BaselineDiffView(QWidget* parent) : QWidget(parent) {
    title_ = new QLabel("Baseline diff — no baselines selected", this);
    QFont f = title_->font();
    f.setBold(true);
    f.setPointSize(f.pointSize() + 2);
    title_->setFont(f);

    table_ = new QTableWidget(0, 5, this);
    table_->setHorizontalHeaderLabels(
        {"Kind", "External ID", "Entity ID", "Changes", "Rollback"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(title_);
    layout->addWidget(table_);
}

void BaselineDiffView::setWiring(tracelink::UiWiringService* wiring) {
    wiring_ = wiring;
}

void BaselineDiffView::loadDiff(const std::string& aId, const std::string& bId,
                                const std::string& rollbackBaselineId) {
    rows_.clear();
    rollbackBaselineId_ = rollbackBaselineId;

    if (wiring_) {
        auto diff = wiring_->visualDiff(aId, bId);
        if (diff.isOk()) {
            rows_ = diff.value();
        }
    }

    title_->setText(QString("Baseline diff — %1 change(s)").arg(rows_.size()));
    rebuildTable();
}

void BaselineDiffView::clear() {
    rows_.clear();
    rollbackBaselineId_.clear();
    title_->setText("Baseline diff — no baselines selected");
    rebuildTable();
}

void BaselineDiffView::rebuildTable() {
    table_->setRowCount(static_cast<int>(rows_.size()));
    for (int i = 0; i < static_cast<int>(rows_.size()); ++i) {
        const auto& row = rows_[static_cast<size_t>(i)];

        auto* kind = new QTableWidgetItem(QString::fromStdString(row.kind));
        table_->setItem(i, 0, kind);

        auto* ext = new QTableWidgetItem(QString::fromStdString(row.entityExternalId));
        table_->setItem(i, 1, ext);

        auto* id = new QTableWidgetItem(QString::fromStdString(row.entityId));
        table_->setItem(i, 2, id);

        QString changes;
        for (const auto& fc : row.fieldChanges) {
            if (!changes.isEmpty()) changes += "\n";
            changes += QString::fromStdString(fc.field) + ": " +
                       QString::fromStdString(fc.oldValue) + " -> " +
                       QString::fromStdString(fc.newValue);
        }
        table_->setItem(i, 3, new QTableWidgetItem(changes));

        auto* btn = new QPushButton("Rollback", table_);
        btn->setEnabled(row.kind == "modified" || row.kind == "removed");
        connect(btn, &QPushButton::clicked, this, [this, i]() { onRollback(i); });
        table_->setCellWidget(i, 4, btn);
    }
}

void BaselineDiffView::onRollback(int row) {
    if (!wiring_ || row < 0 || row >= static_cast<int>(rows_.size())) return;
    if (rollbackBaselineId_.empty()) return;

    const auto& r = rows_[static_cast<size_t>(row)];
    auto type = wiring_->entityTypeOf(r.entityId);
    if (type.failed()) return;

    auto res = wiring_->rollbackEntity(type.value(), r.entityId, rollbackBaselineId_);
    if (res.isOk() && res.value().restored) {
        // Reflect the restored state: re-render the current rows.
        rebuildTable();
    }
}

}  // namespace lodestar::ui
