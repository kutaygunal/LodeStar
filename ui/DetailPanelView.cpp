// ui/DetailPanelView.cpp
// Renders the right-side detail/properties panel for one selected entity:
// its identity + properties and its Active incoming/outgoing links.

#include "ui/DetailPanelView.h"

#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

namespace lodestar::ui {

DetailPanelView::DetailPanelView(QWidget* parent) : QWidget(parent) {
    title_ = new QLabel("No selection", this);
    QFont f = title_->font();
    f.setBold(true);
    f.setPointSize(f.pointSize() + 2);
    title_->setFont(f);

    form_ = new QFormLayout;
    form_->setLabelAlignment(Qt::AlignLeft);

    incoming_ = new QListWidget(this);
    incoming_->setMaximumHeight(120);
    outgoing_ = new QListWidget(this);
    outgoing_->setMaximumHeight(120);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(title_);
    layout->addLayout(form_);
    layout->addWidget(new QLabel("Incoming links:", this));
    layout->addWidget(incoming_);
    layout->addWidget(new QLabel("Outgoing links:", this));
    layout->addWidget(outgoing_);
    layout->addStretch();
}

void DetailPanelView::setModel(const tracelink::DetailPanelModel& model) {
    title_->setText(QString::fromStdString(model.externalId) + "  (" +
                    QString::fromStdString(model.type) + ")");

    // Rebuild the form rows.
    while (form_->rowCount() > 0) form_->removeRow(0);
    auto addRow = [this](const char* label, const std::string& value) {
        form_->addRow(QString::fromLatin1(label),
                      new QLabel(QString::fromStdString(value), this));
    };
    addRow("Name", model.name);
    addRow("Status", model.status);
    addRow("Owner", model.owner);
    addRow("Priority", model.priority);
    addRow("Verification", model.verificationMethod);
    addRow("Safety level", model.safetyLevel);
    addRow("Version", std::to_string(model.version));

    incoming_->clear();
    for (const auto& l : model.incomingLinks) {
        incoming_->addItem(QString::fromStdString(l));
    }
    outgoing_->clear();
    for (const auto& l : model.outgoingLinks) {
        outgoing_->addItem(QString::fromStdString(l));
    }
}

void DetailPanelView::clear() {
    title_->setText("No selection");
    while (form_->rowCount() > 0) form_->removeRow(0);
    incoming_->clear();
    outgoing_->clear();
}

}  // namespace lodestar::ui
