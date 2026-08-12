#pragma once
// ui/CoverageDashboardView.h
// Qt view for the coverage + compliance dashboard. Data comes from the
// Qt-independent CoverageDashboardModel.

#include <QWidget>

class QTableWidget;
class QLabel;

#include "core/tracelink/ViewModelFactory.h"

namespace lodestar::ui {

class CoverageDashboardView : public QWidget {
    Q_OBJECT
public:
    explicit CoverageDashboardView(QWidget* parent = nullptr);

    void setModel(const tracelink::CoverageDashboardModel& model);

private:
    QTableWidget* table_;
    QLabel* summary_;
};

}  // namespace lodestar::ui
