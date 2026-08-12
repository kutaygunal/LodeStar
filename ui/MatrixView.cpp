// ui/MatrixView.cpp
// Qt view for the trace matrix. Rebuilds a QTableWidget from the Qt-independent
// MatrixViewModel and exposes CSV/HTML export.

#include "ui/MatrixView.h"

#include <QFile>
#include <QFileDialog>
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
