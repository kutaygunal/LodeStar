#pragma once
// ui/CoverageDashboardView.h
// Qt view for the coverage + compliance dashboard. Data comes from the
// Qt-independent CoverageDashboardModel.

#include <QWidget>

class QTableWidget;
class QLabel;
class QTextEdit;

#include "core/tracelink/UiWiringService.h"
#include "core/tracelink/ViewModelFactory.h"

namespace lodestar::ui {

class CoverageDashboardView : public QWidget {
    Q_OBJECT
public:
    explicit CoverageDashboardView(QWidget* parent = nullptr);

    void setModel(const tracelink::CoverageDashboardModel& model);

    // WP-7: renders the live coverage dashboard (red/green gaps) from the
    // Qt-independent liveCoverage() wiring.
    void setLiveCoverage(const std::vector<tracelink::LiveCoverageRow>& rows);

    // WP-7: renders the status / priority / coverage charts from the
    // Qt-independent coverageCharts() wiring.
    void setCharts(const tracelink::CoverageCharts& charts);

private:
    QTableWidget* table_;
    QLabel* summary_;
    QTableWidget* liveTable_;
    QTextEdit* charts_;
};

}  // namespace lodestar::ui
