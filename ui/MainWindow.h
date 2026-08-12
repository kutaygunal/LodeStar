#pragma once
// ui/MainWindow.h
// Assembles the four WP-7 Qt views into a tabbed window and refreshes them
// from the Qt-independent ViewModelFactory on load / refresh.

#include <QMainWindow>

#include "core/persistence/Database.h"
#include "core/tracelink/ViewModelFactory.h"

class QTabWidget;
class QToolBar;

namespace lodestar::ui {

class MatrixView;
class GraphView;
class ImpactView;
class CoverageDashboardView;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(lodestar::persistence::Database& db, QWidget* parent = nullptr);

    void refreshAll();

private slots:
    void refresh();

private:
    lodestar::persistence::Database& db_;
    lodestar::tracelink::ViewModelFactory factory_;

    QTabWidget* tabs_;
    MatrixView* matrix_;
    GraphView* graph_;
    ImpactView* impact_;
    CoverageDashboardView* dashboard_;
};

}  // namespace lodestar::ui
