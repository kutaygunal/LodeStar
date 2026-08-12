// ui/MatrixView.cpp
// Qt view for the trace matrix. Rebuilds a QTableWidget from the Qt-independent
// MatrixViewModel and exposes CSV/HTML export.

#include "ui/MatrixView.h"

#include <algorithm>

#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QToolBar>
#include <QVBoxLayout>

namespace lodestar::ui {

MatrixView::MatrixView(QWidget* parent) : QTableWidget(parent) {
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectItems);
    horizontalHeader()->setStretchLastSection(true);
}

void MatrixView::setModel(const tracelink::MatrixViewModel& model) {
    model_ = model;
    clearContents();

    setColumnCount(model.columnCount() + 1);
    setRowCount(model.rowCount());

    setHorizontalHeaderItem(0, new QTableWidgetItem("Requirement"));
    for (int c = 0; c < model.columnCount(); ++c) {
        const auto& col = model.columns[static_cast<size_t>(c)];
        setHorizontalHeaderItem(c + 1, new QTableWidgetItem(QString::fromStdString(col.name)));
    }

    for (int r = 0; r < model.rowCount(); ++r) {
        const auto& row = model.rows[static_cast<size_t>(r)];
        setItem(r, 0,
                new QTableWidgetItem(QString::fromStdString(row.requirementExternalId)));
        for (int c = 0; c < model.columnCount(); ++c) {
            const auto rel = model.cell(r, c);
            auto* item = new QTableWidgetItem(QString::fromStdString(rel));
            if (!rel.empty()) item->setBackground(QColor(0xDCEBFF));
            setItem(r, c + 1, item);
        }
    }
    resizeColumnsToContents();
}

void MatrixView::setWiringService(tracelink::UiWiringService* wiring) {
    wiring_ = wiring;
}

void MatrixView::setSearch(const QString& text) {
    cfg_.search = text.toStdString();
    refreshFiltered();
}

void MatrixView::setStatusFilter(const QString& status) {
    cfg_.statusFilter = status.toStdString();
    refreshFiltered();
}

void MatrixView::toggleRelation(const QString& relation, bool visible) {
    const std::string rel = relation.toStdString();
    auto& hidden = cfg_.hiddenRelations;
    auto it = std::find(hidden.begin(), hidden.end(), rel);
    if (visible) {
        if (it != hidden.end()) hidden.erase(it);
    } else {
        if (it == hidden.end()) hidden.push_back(rel);
    }
    refreshFiltered();
}

void MatrixView::saveView(const QString& name) {
    if (!wiring_) return;
    wiring_->saveMatrixView(name.toStdString(), cfg_);
}

void MatrixView::applyView(const QString& viewId) {
    if (!wiring_) return;
    auto m = wiring_->applyMatrixView(viewId.toStdString());
    if (m.isOk()) setModel(m.value());
}

void MatrixView::refreshFiltered() {
    if (!wiring_) return;
    auto m = wiring_->matrixFiltered(cfg_);
    if (m.isOk()) setModel(m.value());
}

void MatrixView::exportCsv() {
    auto path = QFileDialog::getSaveFileName(this, "Export Matrix (CSV)", "matrix.csv",
                                             "CSV (*.csv)");
    if (path.isEmpty()) return;
    auto csv = model_.toCsv();
    if (!csv.isOk()) {
        QMessageBox::warning(this, "Export", QString::fromStdString(csv.error()));
        return;
    }
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(csv.value().data(), static_cast<qint64>(csv.value().size()));
    }
}

void MatrixView::exportHtml() {
    auto path = QFileDialog::getSaveFileName(this, "Export Matrix (HTML)", "matrix.html",
                                             "HTML (*.html)");
    if (path.isEmpty()) return;
    auto html = model_.toHtml();
    if (!html.isOk()) {
        QMessageBox::warning(this, "Export", QString::fromStdString(html.error()));
        return;
    }
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(html.value().data(), static_cast<qint64>(html.value().size()));
    }
}

}  // namespace lodestar::ui
