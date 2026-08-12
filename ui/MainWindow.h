#pragma once
// ui/MainWindow.h
// Assembles the four WP-7 Qt views into a tabbed window and refreshes them
// from the Qt-independent ViewModelFactory on load / refresh.

#include <QMainWindow>

#include "core/persistence/Database.h"
#include "core/tracelink/UiWiringService.h"

class QTabWidget;
class QToolBar;
class QSplitter;

namespace lodestar::ui {

class MatrixView;
class GraphView;
class ImpactView;
class CoverageDashboardView;
class ProjectTreeView;
class DetailPanelView;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(lodestar::persistence::Database& db, QWidget* parent = nullptr);

    void refreshAll();

    // Populates the right-side detail panel for one entity.
    void showDetail(lodestar::tracelink::EntityType type, const std::string& id);

private slots:
    void refresh();

private:
    lodestar::persistence::Database& db_;
    lodestar::tracelink::UiWiringService wiring_;

    QSplitter* splitter_;
    ProjectTreeView* tree_;
    QTabWidget* tabs_;
    MatrixView* matrix_;
    GraphView* graph_;
    ImpactView* impact_;
    CoverageDashboardView* dashboard_;
    DetailPanelView* detail_;
};

}  // namespace lodestar::ui
