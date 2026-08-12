// ui/CoverageDashboardView.cpp
// Shows per-requirement coverage percentages plus the compliance violation
// count from the latest validation run.

#include "ui/CoverageDashboardView.h"

#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

namespace lodestar::ui {

CoverageDashboardView::CoverageDashboardView(QWidget* parent)
    : QWidget(parent), table_(new QTableWidget(this)), summary_(new QLabel(this)) {
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels(
        {"Requirement", "% Designed", "% Verified", "% Satisfied"});

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(summary_);
    layout->addWidget(table_);
}

void CoverageDashboardView::setModel(const tracelink::CoverageDashboardModel& model) {
    summary_->setText(QString("Overall designed: %1%   verified: %2%   violations: %3")
                          .arg(model.overallPercentDesigned)
                          .arg(model.overallPercentVerified)
                          .arg(model.violationCount));

    table_->setRowCount(static_cast<int>(model.items.size()));
    for (int i = 0; i < static_cast<int>(model.items.size()); ++i) {
        const auto& it = model.items[static_cast<size_t>(i)];
        table_->setItem(i, 0,
                        new QTableWidgetItem(QString::fromStdString(it.requirementExternalId)));
        table_->setItem(i, 1, new QTableWidgetItem(QString::number(it.percentDesigned)));
        table_->setItem(i, 2, new QTableWidgetItem(QString::number(it.percentVerified)));
        table_->setItem(i, 3, new QTableWidgetItem(QString::number(it.percentSatisfied)));
    }
    table_->resizeColumnsToContents();
}

}  // namespace lodestar::ui
