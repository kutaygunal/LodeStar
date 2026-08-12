#pragma once
// ui/MatrixView.h
// Qt view for the trace matrix (rows = requirements, columns = design + test).
// Built only when LODESTAR_BUILD_UI=ON. Data comes from the Qt-independent
// lodestar::tracelink::MatrixViewModel.

#include <QTableWidget>

#include "core/tracelink/UiWiringService.h"
#include "core/tracelink/ViewModelFactory.h"

namespace lodestar::ui {

class MatrixView : public QTableWidget {
    Q_OBJECT
public:
    explicit MatrixView(QWidget* parent = nullptr);

    // Rebuild the table + export buttons from a freshly built view model.
    void setModel(const tracelink::MatrixViewModel& model);

    // Export handlers (wired to the export buttons).
    void exportCsv();
    void exportHtml();

    // WP-8: interactive matrix wiring. The MainWindow supplies the wiring
    // service so search/filter/toggle and saved views hit the same
    // Qt-independent layer the tests verify.
    void setWiringService(tracelink::UiWiringService* wiring);

    // Applies a search substring (name/externalId) and refreshes the table.
    void setSearch(const QString& text);

    // Applies a status filter ("" = all) and refreshes the table.
    void setStatusFilter(const QString& status);

    // Toggles a relation column on/off and refreshes the table.
    void toggleRelation(const QString& relation, bool visible);

    // Saves the current view under a name.
    void saveView(const QString& name);

    // Applies a saved view by id and refreshes the table.
    void applyView(const QString& viewId);

private:
    void refreshFiltered();

    tracelink::MatrixViewModel model_;
    tracelink::UiWiringService* wiring_ = nullptr;
    tracelink::MatrixViewConfig cfg_;
};

}  // namespace lodestar::ui
