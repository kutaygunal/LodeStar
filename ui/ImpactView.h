#pragma once
// ui/ImpactView.h
// Qt view for impact analysis: an affected-entity tree plus blocked
// transitions. Data comes from the Qt-independent ImpactViewModel.

#include <QWidget>

class QTreeWidget;
class QLabel;
class QListWidget;

#include "core/tracelink/ViewModelFactory.h"

namespace lodestar::ui {

class ImpactView : public QWidget {
    Q_OBJECT
public:
    explicit ImpactView(QWidget* parent = nullptr);

    void setModel(const tracelink::ImpactViewModel& model);

private:
    QTreeWidget* tree_;
    QListWidget* transitions_;
    QLabel* summary_;
};

}  // namespace lodestar::ui
