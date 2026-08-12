#pragma once
// ui/DocumentView.h
// Qt view for document-style authoring (WP-10). Renders a document (sections +
// requirements) from the Qt-independent UiWiringService::document()
// (DocumentModel) and calls addRequirementToDocument() / reorderRequirements()
// to author requirements in a document context with atomic traceability.
// Built only when LODESTAR_BUILD_UI=ON.

#include <QWidget>

#include "core/tracelink/UiWiringService.h"

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QComboBox;

namespace lodestar::ui {

class DocumentView : public QWidget {
    Q_OBJECT
public:
    explicit DocumentView(lodestar::tracelink::UiWiringService& wiring,
                          QWidget* parent = nullptr);

    // Load and render the document rooted at `docId`. Fails cleanly (clears the
    // view) if the document root is missing.
    void setDocument(const std::string& docId);

    // Author a new requirement into `sectionId` of the current document.
    // Returns true on success (and refreshes the view).
    bool addRequirement(const std::string& sectionId,
                        const lodestar::tracelink::Entity& req);

    // Reorder the requirements of `sectionId` to the given id order.
    // Returns true on success (and refreshes the view).
    bool reorderRequirements(const std::string& sectionId,
                             const std::vector<std::string>& orderedIds);

private slots:
    void onAddClicked();

private:
    void refresh();
    void addSectionItem(QTreeWidgetItem* parent,
                        const lodestar::tracelink::DocumentSection& section);

    lodestar::tracelink::UiWiringService& wiring_;
    std::string docId_;

    QTreeWidget* tree_;
    QComboBox* sectionCombo_;
    QLineEdit* nameEdit_;
    QLineEdit* textEdit_;
};

}  // namespace lodestar::ui
