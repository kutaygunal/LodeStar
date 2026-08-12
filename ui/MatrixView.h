#pragma once
// ui/MatrixView.h
// Qt view for the trace matrix (rows = requirements, columns = design + test).
// Built only when LODESTAR_BUILD_UI=ON. Data comes from the Qt-independent
// lodestar::tracelink::MatrixViewModel.

#include <QTableWidget>

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

private:
    tracelink::MatrixViewModel model_;
};

}  // namespace lodestar::ui
