// ui/MainWindow.cpp
// Wires the four WP-7 views together and refreshes them from the view models.

#include "ui/MainWindow.h"

#include <QAction>
#include <QTabWidget>
#include <QToolBar>

#include "ui/CoverageDashboardView.h"
#include "ui/GraphView.h"
#include "ui/ImpactView.h"
#include "ui/MatrixView.h"

namespace lodestar::ui {

MainWindow::MainWindow(lodestar::persistence::Database& db, QWidget* parent)
    : QMainWindow(parent), db_(db), factory_(db) {
    setWindowTitle("Lodestar — Trace Link");
    resize(1100, 700);

    tabs_ = new QTabWidget(this);
    matrix_ = new MatrixView(tabs_);
    graph_ = new GraphView(tabs_);
    impact_ = new ImpactView(tabs_);
    dashboard_ = new CoverageDashboardView(tabs_);

    tabs_->addTab(matrix_, "Trace Matrix");
    tabs_->addTab(graph_, "Graph");
    tabs_->addTab(impact_, "Impact");
    tabs_->addTab(dashboard_, "Coverage Dashboard");
    setCentralWidget(tabs_);

    auto* bar = addToolBar("Main");
    auto* refreshAction = bar->addAction("Refresh");
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, &MainWindow::refresh);

    auto* exportCsv = bar->addAction("Export Matrix (CSV)");
    connect(exportCsv, &QAction::triggered, matrix_, &MatrixView::exportCsv);
    auto* exportHtml = bar->addAction("Export Matrix (HTML)");
    connect(exportHtml, &QAction::triggered, matrix_, &MatrixView::exportHtml);

    refresh();
}

void MainWindow::refreshAll() { refresh(); }

void MainWindow::refresh() {
    auto m = factory_.matrix();
    if (m.isOk()) matrix_->setModel(m.value());

    auto g = factory_.graph();
    if (g.isOk()) graph_->setModel(g.value());

    auto d = factory_.coverageDashboard();
    if (d.isOk()) dashboard_->setModel(d.value());

    // Impact view needs a focus entity; default to the first requirement.
    auto firstReq = factory_.matrix();
    if (firstReq.isOk() && !firstReq.value().rows.empty()) {
        auto imp = factory_.impact(lodestar::tracelink::EntityType::Requirement,
                                   firstReq.value().rows.front().requirementId);
        if (imp.isOk()) impact_->setModel(imp.value());
    }
}

}  // namespace lodestar::ui
