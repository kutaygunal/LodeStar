#pragma once
// ui/GraphView.h
// Qt view that renders the graph node-link diagram on a QGraphicsScene.
// Data comes from the Qt-independent lodestar::tracelink::GraphViewModel.

#include <QGraphicsView>

#include "core/tracelink/ViewModelFactory.h"

class QGraphicsScene;

namespace lodestar::ui {

class GraphView : public QGraphicsView {
    Q_OBJECT
public:
    explicit GraphView(QWidget* parent = nullptr);
    ~GraphView() override;

    void setModel(const tracelink::GraphViewModel& model);

private:
    QGraphicsScene* scene_;
};

}  // namespace lodestar::ui
