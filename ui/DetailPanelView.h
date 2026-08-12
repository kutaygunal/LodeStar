#pragma once
// ui/DetailPanelView.h
// Qt view for the right-side detail/properties panel. Renders one entity's
// properties + Active incoming/outgoing links from the Qt-independent
// UiWiringService::detail() (DetailPanelModel). Built only when
// LODESTAR_BUILD_UI=ON.

#include <QWidget>

class QLabel;
class QFormLayout;
class QListWidget;

#include "core/tracelink/UiWiringService.h"

namespace lodestar::ui {

class DetailPanelView : public QWidget {
    Q_OBJECT
public:
    explicit DetailPanelView(QWidget* parent = nullptr);

    // Render the detail/properties panel for one entity.
    void setModel(const tracelink::DetailPanelModel& model);

    // Clear the panel (e.g. when nothing is selected).
    void clear();

private:
    QLabel* title_;
    QFormLayout* form_;
    QListWidget* incoming_;
    QListWidget* outgoing_;
};

}  // namespace lodestar::ui
