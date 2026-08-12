// ui/GraphView.cpp
// Renders the trace graph as node ellipses and typed edges on a QGraphicsScene.

#include "ui/GraphView.h"

#include <QGraphicsEllipseItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>

#include <cmath>
#include <map>

namespace lodestar::ui {

namespace {
constexpr double kRadius = 28.0;
constexpr double kRing = 150.0;

QColor typeColor(const QString& type) {
    if (type == "requirement") return QColor(0x4F81BD);
    if (type == "design") return QColor(0x9BBB59);
    if (type == "test_case") return QColor(0xC0504D);
    return QColor(0x8064A2);
}
}  // namespace

GraphView::GraphView(QWidget* parent)
    : QGraphicsView(parent), scene_(new QGraphicsScene(this)) {
    setScene(scene_);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
}

GraphView::~GraphView() = default;

void GraphView::setModel(const tracelink::GraphViewModel& model) {
    scene_->clear();

    // Place nodes on a circle for a clean layout.
    const int n = static_cast<int>(model.nodes.size());
    std::map<std::string, QPointF> pos;
    std::map<std::string, QString> name;
    for (int i = 0; i < n; ++i) {
        const auto& node = model.nodes[static_cast<size_t>(i)];
        const double angle = (2.0 * M_PI * static_cast<double>(i)) / static_cast<double>(n);
        pos[node.id] = QPointF(std::cos(angle) * kRing, std::sin(angle) * kRing);
        name[node.id] = QString::fromStdString(node.externalId);
    }

    // Edges first (under the nodes).
    for (const auto& e : model.edges) {
        auto sIt = pos.find(e.sourceId);
        auto tIt = pos.find(e.targetId);
        if (sIt == pos.end() || tIt == pos.end()) continue;
        scene_->addLine(QLineF(sIt->second, tIt->second), QPen(QColor(0x999999), 1.5));
    }

    // Nodes.
    for (const auto& node : model.nodes) {
        auto it = pos.find(node.id);
        if (it == pos.end()) continue;
        auto* ellipse = scene_->addEllipse(it->second.x() - kRadius,
                                           it->second.y() - kRadius,
                                           2 * kRadius, 2 * kRadius,
                                           QPen(Qt::black),
                                           QBrush(typeColor(QString::fromStdString(node.type))));
        ellipse->setFlag(QGraphicsItem::ItemIsMovable);
        scene_->addText(name[node.id])->setPos(it->second.x() - kRadius / 2,
                                               it->second.y() - 6);
    }
    fitInView(scene_->itemsBoundingRect(), Qt::KeepAspectRatio);
}

}  // namespace lodestar::ui
