#pragma once
// ui/ProjectTreeView.h
// Qt view for the left-nav project tree. Renders the nested hierarchy from
// the Qt-independent UiWiringService::projectTree() (ProjectTreeNode). Built
// only when LODESTAR_BUILD_UI=ON.

#include <QTreeView>

#include "core/tracelink/UiWiringService.h"

class QStandardItemModel;

namespace lodestar::ui {

class ProjectTreeView : public QTreeView {
    Q_OBJECT
public:
    explicit ProjectTreeView(QWidget* parent = nullptr);

    // Rebuild the tree from a freshly built project tree (vector of roots).
    void setModel(const std::vector<tracelink::ProjectTreeNode>& roots);

signals:
    // Emitted when the user selects a node: (type, id).
    void entitySelected(const QString& type, const QString& id);

private:
    QStandardItemModel* model_;
};

}  // namespace lodestar::ui
