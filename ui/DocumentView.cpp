// ui/DocumentView.cpp
// Renders a document (sections + requirements) from UiWiringService::document()
// and authors new requirements via addRequirementToDocument() /
// reorderRequirements().

#include "ui/DocumentView.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace lodestar::ui {

DocumentView::DocumentView(lodestar::tracelink::UiWiringService& wiring,
                           QWidget* parent)
    : QWidget(parent), wiring_(wiring) {
    tree_ = new QTreeWidget(this);
    tree_->setHeaderLabels({"Document"});
    tree_->setColumnCount(1);

    // Authoring controls: pick a section, enter a name + body, add.
    sectionCombo_ = new QComboBox(this);
    nameEdit_ = new QLineEdit(this);
    nameEdit_->setPlaceholderText("Requirement name");
    textEdit_ = new QLineEdit(this);
    textEdit_->setPlaceholderText("Requirement body");
    auto* addBtn = new QPushButton("Add requirement", this);
    connect(addBtn, &QPushButton::clicked, this, &DocumentView::onAddClicked);

    auto* authorRow = new QHBoxLayout;
    authorRow->addWidget(new QLabel("Section:", this));
    authorRow->addWidget(sectionCombo_, 1);
    authorRow->addWidget(nameEdit_, 1);
    authorRow->addWidget(textEdit_, 2);
    authorRow->addWidget(addBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tree_, 1);
    layout->addLayout(authorRow);
}

void DocumentView::setDocument(const std::string& docId) {
    docId_ = docId;
    refresh();
}

bool DocumentView::addRequirement(const std::string& sectionId,
                                  const lodestar::tracelink::Entity& req) {
    if (docId_.empty()) return false;
    auto res = wiring_.addRequirementToDocument(docId_, sectionId, req);
    if (res.failed()) return false;
    refresh();
    return true;
}

bool DocumentView::reorderRequirements(
    const std::string& sectionId, const std::vector<std::string>& orderedIds) {
    if (docId_.empty()) return false;
    auto res = wiring_.reorderRequirements(docId_, sectionId, orderedIds);
    if (res.failed()) return false;
    refresh();
    return true;
}

void DocumentView::onAddClicked() {
    const int idx = sectionCombo_->currentIndex();
    if (idx < 0) return;
    const std::string sectionId = sectionCombo_->itemData(idx).toString().toStdString();
    if (sectionId.empty()) return;

    lodestar::tracelink::Entity req;
    req.type = lodestar::tracelink::EntityType::Requirement;
    req.externalId = nameEdit_->text().toStdString();
    req.name = nameEdit_->text().toStdString();
    req.text = textEdit_->text().toStdString();
    req.status = "Draft";
    if (req.name.empty()) return;

    if (addRequirement(sectionId, req)) {
        nameEdit_->clear();
        textEdit_->clear();
    }
}

void DocumentView::addSectionItem(
    QTreeWidgetItem* parent, const lodestar::tracelink::DocumentSection& section) {
    auto* secItem = new QTreeWidgetItem(parent);
    secItem->setText(0, QString::fromStdString(section.title));
    secItem->setData(0, Qt::UserRole, QString::fromStdString(section.id));
    for (const auto& r : section.requirements) {
        auto* reqItem = new QTreeWidgetItem(secItem);
        reqItem->setText(0, QString::fromStdString(r.externalId) + "  —  " +
                               QString::fromStdString(r.name));
        reqItem->setData(0, Qt::UserRole, QString::fromStdString(r.id));
    }
}

void DocumentView::refresh() {
    tree_->clear();
    sectionCombo_->clear();
    if (docId_.empty()) return;

    auto doc = wiring_.document(docId_);
    if (doc.failed()) return;

    const auto& model = doc.value();
    auto* root = new QTreeWidgetItem(tree_);
    root->setText(0, QString::fromStdString(model.title));
    for (const auto& s : model.sections) {
        addSectionItem(root, s);
        sectionCombo_->addItem(QString::fromStdString(s.title),
                               QString::fromStdString(s.id));
    }
    root->setExpanded(true);
}

}  // namespace lodestar::ui
