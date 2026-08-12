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
    : QMainWindow(parent), db_(db), wiring_(db) {
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
    // Single-pass refresh through the WP-G wiring service: builds all four
    // view models consistently (matrix rows == coverage items == number of
    // requirements; graph nodes == all active entities).
    auto snap = wiring_.refreshAll();
    if (!snap.isOk()) return;
    const auto& s = snap.value();

    matrix_->setModel(s.matrix);
    graph_->setModel(s.graph);
    dashboard_->setModel(s.coverage);

    // Impact view needs a focus entity; default to the first requirement.
    if (!s.impacts.empty()) impact_->setModel(s.impacts.front());
}

}  // namespace lodestar::ui
