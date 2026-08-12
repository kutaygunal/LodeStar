// ui/MainWindow.cpp
// Wires the four WP-7 views together and refreshes them from the view models.

#include "ui/MainWindow.h"

#include <QAction>
#include <QSplitter>
#include <QTabWidget>
#include <QToolBar>

#include "ui/CoverageDashboardView.h"
#include "ui/DetailPanelView.h"
#include "ui/GraphView.h"
#include "ui/ImpactView.h"
#include "ui/MatrixView.h"
#include "ui/ProjectTreeView.h"
#include "ui/DocumentView.h"
#include "ui/BaselineDiffView.h"

namespace lodestar::ui {

MainWindow::MainWindow(lodestar::persistence::Database& db, QWidget* parent)
    : QMainWindow(parent), db_(db), wiring_(db) {
    setWindowTitle("Lodestar — Trace Link");
    resize(1200, 750);

    // Left-nav project tree.
    tree_ = new ProjectTreeView(this);

    // Right side: existing tabs on top, detail panel below.
    tabs_ = new QTabWidget(this);
    matrix_ = new MatrixView(tabs_);
    graph_ = new GraphView(tabs_);
    impact_ = new ImpactView(tabs_);
    dashboard_ = new CoverageDashboardView(tabs_);
    diff_ = new BaselineDiffView(tabs_);
    diff_->setWiring(&wiring_);
    document_ = new DocumentView(wiring_, tabs_);

    matrix_->setWiringService(&wiring_);
    tabs_->addTab(matrix_, "Trace Matrix");
    tabs_->addTab(graph_, "Graph");
    tabs_->addTab(impact_, "Impact");
    tabs_->addTab(dashboard_, "Coverage Dashboard");
    tabs_->addTab(diff_, "Baseline Diff");
    tabs_->addTab(document_, "Document");

    detail_ = new DetailPanelView(this);
    auto* right = new QSplitter(Qt::Vertical, this);
    right->addWidget(tabs_);
    right->addWidget(detail_);
    right->setStretchFactor(0, 3);
    right->setStretchFactor(1, 1);

    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->addWidget(tree_);
    splitter_->addWidget(right);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 4);
    setCentralWidget(splitter_);

    // Selecting a node in the tree shows its detail panel.
    connect(tree_, &ProjectTreeView::entitySelected, this,
            [this](const QString& type, const QString& id) {
                auto t = lodestar::tracelink::entityTypeFromString(type.toStdString());
                if (t) showDetail(*t, id.toStdString());
            });

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

void MainWindow::showDetail(lodestar::tracelink::EntityType type,
                            const std::string& id) {
    auto d = wiring_.detail(type, id);
    if (d.isOk()) {
        detail_->setModel(d.value());
    } else {
        detail_->clear();
    }
}

void MainWindow::showBaselineDiff(const std::string& aId, const std::string& bId,
                                  const std::string& rollbackBaselineId) {
    diff_->loadDiff(aId, bId, rollbackBaselineId);
}

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

    // WP-7: live coverage dashboard (red/green gaps) + charts.
    auto live = wiring_.liveCoverage();
    if (live.isOk()) dashboard_->setLiveCoverage(live.value());
    auto charts = wiring_.coverageCharts();
    if (charts.isOk()) dashboard_->setCharts(charts.value());

    // Impact view needs a focus entity; default to the first requirement.
    if (!s.impacts.empty()) impact_->setModel(s.impacts.front());

    // Left-nav project tree.
    auto tree = wiring_.projectTree();
    if (tree.isOk()) tree_->setModel(tree.value());
}

}  // namespace lodestar::ui
