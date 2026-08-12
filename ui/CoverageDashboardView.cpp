// ui/CoverageDashboardView.cpp
// Shows per-requirement coverage percentages plus the compliance violation
// count from the latest validation run. WP-7 extends it with the live coverage
// dashboard (red/green gaps) and the status/priority/coverage charts, all fed
// from the Qt-independent UiWiringService wiring.

#include "ui/CoverageDashboardView.h"

#include <QLabel>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

namespace lodestar::ui {

CoverageDashboardView::CoverageDashboardView(QWidget* parent)
    : QWidget(parent),
      table_(new QTableWidget(this)),
      summary_(new QLabel(this)),
      liveTable_(new QTableWidget(this)),
      charts_(new QTextEdit(this)) {
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels(
        {"Requirement", "% Designed", "% Verified", "% Satisfied"});

    // WP-7 live dashboard: red/green gaps.
    liveTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    liveTable_->setColumnCount(5);
    liveTable_->setHorizontalHeaderLabels(
        {"Requirement", "Designed", "Verified", "Gap: Design", "Gap: Test"});

    charts_->setReadOnly(true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(summary_);
    layout->addWidget(table_);
    layout->addWidget(new QLabel("Live Coverage (red/green gaps)", this));
    layout->addWidget(liveTable_);
    layout->addWidget(new QLabel("Charts", this));
    layout->addWidget(charts_);
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

void CoverageDashboardView::setLiveCoverage(
    const std::vector<tracelink::LiveCoverageRow>& rows) {
    liveTable_->setRowCount(static_cast<int>(rows.size()));
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const auto& r = rows[static_cast<size_t>(i)];
        liveTable_->setItem(i, 0,
                            new QTableWidgetItem(QString::fromStdString(r.requirementExternalId)));
        liveTable_->setItem(i, 1, new QTableWidgetItem(r.designed ? "Yes" : "No"));
        liveTable_->setItem(i, 2, new QTableWidgetItem(r.verified ? "Yes" : "No"));
        liveTable_->setItem(i, 3, new QTableWidgetItem(r.gapNoDesign ? "RED" : ""));
        liveTable_->setItem(i, 4, new QTableWidgetItem(r.gapNoTest ? "RED" : ""));
    }
    liveTable_->resizeColumnsToContents();
}

void CoverageDashboardView::setCharts(const tracelink::CoverageCharts& charts) {
    QString text;
    text += "By Status:\n";
    for (const auto& s : charts.byStatus) {
        text += QString("  %1: %2\n").arg(QString::fromStdString(s.label)).arg(s.count);
    }
    text += "By Priority:\n";
    for (const auto& s : charts.byPriority) {
        text += QString("  %1: %2\n").arg(QString::fromStdString(s.label)).arg(s.count);
    }
    text += "By Coverage:\n";
    for (const auto& s : charts.byCoverage) {
        text += QString("  %1: %2\n").arg(QString::fromStdString(s.label)).arg(s.count);
    }
    charts_->setPlainText(text);
}

}  // namespace lodestar::ui
