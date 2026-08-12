#pragma once
// ui/BaselineDiffView.h
// Qt view for the WP-9 baseline visual compare + per-item rollback. Renders
// the rows produced by UiWiringService::visualDiff() (VisualDiffRow) and calls
// UiWiringService::rollbackEntity() for per-item rollback. Built only when
// LODESTAR_BUILD_UI=ON.

#include <QWidget>
#include <string>
#include <vector>

class QTableWidget;
class QLabel;

#include "core/tracelink/UiWiringService.h"

namespace lodestar::ui {

class BaselineDiffView : public QWidget {
    Q_OBJECT
public:
    explicit BaselineDiffView(QWidget* parent = nullptr);

    // The wiring service the view calls for visualDiff() and rollbackEntity().
    void setWiring(tracelink::UiWiringService* wiring);

    // Loads and renders the visual diff of baseline a (older) against b
    // (newer). `rollbackBaselineId` is the baseline a per-item rollback
    // restores to (normally the older baseline).
    void loadDiff(const std::string& aId, const std::string& bId,
                  const std::string& rollbackBaselineId);

    // Clears the table (e.g. when no baselines are selected).
    void clear();

private slots:
    void onRollback(int row);

private:
    void rebuildTable();

    tracelink::UiWiringService* wiring_ = nullptr;
    std::vector<tracelink::VisualDiffRow> rows_;
    std::string rollbackBaselineId_;

    QLabel* title_;
    QTableWidget* table_;
};

}  // namespace lodestar::ui
