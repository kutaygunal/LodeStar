// core/assurecheck/CertReportService.cpp
// S2 Phase 8: certification-ready reporting + traceability implementation.
// S3 Phase 4: depth pass -- see CertReportService.h and PLAN.md ("Phase 4
// scope brief") for what changed and why.
//
// Decision recorded here (PLAN.md asked for one, explicitly, not a silent
// default): PDF/Word/ReQIF stay hand-rolled rather than pulling in a
// generation library via vcpkg. vcpkg.json currently has exactly one
// dependency (sqlite3); this codebase has consistently hand-rolled every
// other binary/text format it produces (SkydelAdapter's HTTP client, the
// OSLC RDF/XML writer, core/testforge/CoberturaImport.cpp's Cobertura
// parser). A real PDF/DOCX library would buy correctness for free but adds a
// new external dependency and a new build-time risk (network-fetched vcpkg
// port) to a project whose whole differentiator is being buildable
// offline/on-prem for an avionics audience. Hand-rolling a *correct*
// multi-page PDF and a *complete* OOXML docx is real, fiddly binary-format
// work, but it is doable -- see buildPdf/buildDocxParts below -- and keeps
// the dependency surface unchanged. If a future phase needs rich text
// layout (word-wrap, images, headers/footers per section) beyond what a
// column/line grid can do, that is the point to revisit this decision.

#include "core/assurecheck/CertReportService.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <sqlite3.h>

#include "core/common/Time.h"

namespace lodestar::assurecheck {

namespace {

// ---------------------------------------------------------------------------
// Small helpers.
// ---------------------------------------------------------------------------

std::string xmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c;
        }
    }
    return out;
}

// Escapes a string for use inside a PDF literal string (parentheses/backslash).
std::string pdfEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '(' || c == ')' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

// Truncates a cell's text to fit `maxChars`, marking truncation with "..."
// (ASCII, not a unicode ellipsis -- the standard-14 fonts used here aren't
// declared with an encoding that's guaranteed to carry U+2026). This is a
// deliberate scope boundary: no word-wrap, just a fixed column grid -- see
// the file header comment on the hand-roll decision.
std::string truncateCell(const std::string& s, std::size_t maxChars) {
    if (s.size() <= maxChars) return s;
    if (maxChars <= 3) return s.substr(0, maxChars);
    return s.substr(0, maxChars - 3) + "...";
}

// Average glyph width for Helvetica/Helvetica-Bold is close to half the
// point size; used to budget how many characters fit a column without
// pulling in real font metrics.
std::size_t charBudget(int colWidthPts, int fontSize) {
    const double avgCharWidth = fontSize * 0.5;
    if (avgCharWidth <= 0.0) return 0;
    return static_cast<std::size_t>(colWidthPts / avgCharWidth);
}

// ---------------------------------------------------------------------------
// Format-agnostic report document: built once from a ComplianceReport (+
// per-row audit trail), then rendered into PDF and DOCX separately. Keeps
// the "what goes in the report" logic out of both binary-format writers.
// ---------------------------------------------------------------------------
struct ReportDoc {
    std::string title;
    std::vector<std::string> metaLines;
    std::vector<std::string> tableHeader;
    std::vector<std::vector<std::string>> tableRows;
    std::vector<std::string> auditLines;
};

ReportDoc buildReportDoc(const ComplianceReport& report,
                         const std::vector<std::vector<AuditEntry>>& auditByRow) {
    ReportDoc doc;
    doc.title = "Compliance Report - " + report.standardCode;
    doc.metaLines.push_back("Standard: " + report.standardCode + " - " +
                            report.standardName);
    doc.metaLines.push_back("Project DAL: " + report.dalLevel);
    doc.metaLines.push_back(
        "Coverage: " + std::to_string(report.coverage.percent) +
        "%  (Pass " + std::to_string(report.coverage.pass) + " / Fail " +
        std::to_string(report.coverage.fail) + " / NA " +
        std::to_string(report.coverage.na) + " / Warning " +
        std::to_string(report.coverage.warning) + ")");

    doc.tableHeader = {"Item", "Status", "DAL", "Objective", "Evidence"};
    for (const auto& row : report.rows) {
        doc.tableRows.push_back(
            {row.itemCode, row.status, row.dalLevel, row.objective,
             row.evidence.empty() ? std::string("(none)") : row.evidence});
    }

    for (std::size_t i = 0; i < report.rows.size() && i < auditByRow.size();
         ++i) {
        const auto& row = report.rows[i];
        for (const auto& e : auditByRow[i]) {
            doc.auditLines.push_back(row.itemCode + ": " + e.actor + " " +
                                     e.action + " (" + e.fromState + " -> " +
                                     e.toState + ") @ " + e.timestamp);
        }
    }
    return doc;
}

// ---------------------------------------------------------------------------
// Minimal but valid multi-page PDF writer.
//
// Layout model: the document is a flat sequence of "units" (a text line, a
// multi-column table row, or a horizontal rule), each consuming exactly one
// line of vertical space. Units are paginated by a simple line-count budget
// per page; if a page break lands inside the checklist table, the header
// row + rule are re-emitted at the top of the continuation page so every
// page's table is self-describing. No word-wrap: long cell text is
// truncated to its column's character budget (see truncateCell/charBudget
// and the hand-roll-decision comment at the top of this file).
// ---------------------------------------------------------------------------
namespace pdf {

struct Unit {
    enum class Kind { Text, TableRow, Rule, Blank } kind;
    std::string text;
    std::vector<std::string> cells;
    bool bold = false;
    int size = 10;
    bool isTableHeader = false;
};

std::vector<Unit> buildUnits(const ReportDoc& doc) {
    std::vector<Unit> units;
    units.push_back({Unit::Kind::Text, doc.title, {}, true, 16, false});
    units.push_back({Unit::Kind::Blank, "", {}, false, 10, false});
    for (const auto& m : doc.metaLines) {
        units.push_back({Unit::Kind::Text, m, {}, false, 11, false});
    }
    units.push_back({Unit::Kind::Blank, "", {}, false, 10, false});
    units.push_back(
        {Unit::Kind::Text, "Checklist Results", {}, true, 12, false});

    Unit header;
    header.kind = Unit::Kind::TableRow;
    header.cells = doc.tableHeader;
    header.bold = true;
    header.size = 9;
    header.isTableHeader = true;
    units.push_back(header);
    units.push_back({Unit::Kind::Rule, "", {}, false, 0, false});

    for (const auto& row : doc.tableRows) {
        units.push_back({Unit::Kind::TableRow, "", row, false, 9, false});
    }

    if (!doc.auditLines.empty()) {
        units.push_back({Unit::Kind::Blank, "", {}, false, 10, false});
        units.push_back({Unit::Kind::Text, "Review & Approval Audit Trail",
                         {}, true, 12, false});
        for (const auto& a : doc.auditLines) {
            units.push_back({Unit::Kind::Text, a, {}, false, 9, false});
        }
    }
    return units;
}

std::string renderTextLine(const std::string& text, bool bold, int size,
                           int x, int y) {
    const char* font = bold ? "/F2" : "/F1";
    return "BT " + std::string(font) + " " + std::to_string(size) +
           " Tf " + std::to_string(x) + " " + std::to_string(y) +
           " Td (" + pdfEscape(text) + ") Tj ET\n";
}

std::string renderTableRow(const std::vector<std::string>& cells, bool bold,
                           int y, const std::vector<int>& colX,
                           const std::vector<int>& colW, int size) {
    std::string out;
    for (std::size_t c = 0; c < cells.size() && c < colX.size(); ++c) {
        const std::string text =
            truncateCell(cells[c], charBudget(colW[c], size));
        out += renderTextLine(text, bold, size, colX[c], y);
    }
    return out;
}

std::string renderRule(int y, int x0, int x1) {
    return "1 w\n" + std::to_string(x0) + " " + std::to_string(y) + " m " +
           std::to_string(x1) + " " + std::to_string(y) + " l S\n";
}

}  // namespace pdf

std::vector<std::uint8_t> buildPdf(const ReportDoc& doc) {
    using pdf::Unit;
    std::vector<Unit> units = pdf::buildUnits(doc);

    // Capture the header-row + rule pair so it can be re-emitted at the top
    // of any continuation page that starts mid-table.
    std::vector<Unit> headerBlock;
    for (std::size_t i = 0; i < units.size(); ++i) {
        if (units[i].kind == Unit::Kind::TableRow && units[i].isTableHeader) {
            headerBlock.push_back(units[i]);
            if (i + 1 < units.size() &&
                units[i + 1].kind == Unit::Kind::Rule) {
                headerBlock.push_back(units[i + 1]);
            }
            break;
        }
    }

    const int pageWidth = 612;
    const int pageHeight = 792;
    const int marginX = 50;
    const int topY = 742;
    const int bottomY = 70;   // leaves room for the page-number footer
    const int lineHeight = 14;
    const int maxLines = (topY - bottomY) / lineHeight;

    std::vector<std::vector<Unit>> pages;
    std::vector<Unit> cur;
    for (std::size_t i = 0; i < units.size(); ++i) {
        if (static_cast<int>(cur.size()) >= maxLines) {
            pages.push_back(cur);
            cur.clear();
            if (!headerBlock.empty() &&
                units[i].kind == Unit::Kind::TableRow &&
                !units[i].isTableHeader) {
                for (const auto& h : headerBlock) cur.push_back(h);
            }
        }
        cur.push_back(units[i]);
    }
    pages.push_back(cur);  // always at least one page, even if empty
    const int pageCount = static_cast<int>(pages.size());

    // Checklist table column geometry: 5 columns across the 512pt content
    // width (612 - 2*50 margin).
    const std::vector<int> colX = {50, 110, 165, 205, 410};
    const std::vector<int> colW = {60, 55, 40, 205, 152};

    // Object id plan: 1=Catalog, 2=Pages, then N Page objects, then N
    // Content-stream objects, then the two shared Font objects.
    const int catalogId = 1;
    const int pagesId = 2;
    const int firstPageId = 3;
    const int firstContentId = firstPageId + pageCount;
    const int fontRegId = firstContentId + pageCount;
    const int fontBoldId = fontRegId + 1;

    std::string out = "%PDF-1.4\n";
    std::vector<std::size_t> offsets(static_cast<std::size_t>(fontBoldId) + 1,
                                     0);

    auto addObj = [&](int id, const std::string& body) {
        offsets[static_cast<std::size_t>(id)] = out.size();
        out += std::to_string(id) + " 0 obj\n" + body + "\nendobj\n";
    };

    addObj(catalogId,
          "<< /Type /Catalog /Pages " + std::to_string(pagesId) + " 0 R >>");

    std::string kids;
    for (int p = 0; p < pageCount; ++p) {
        if (p) kids += " ";
        kids += std::to_string(firstPageId + p) + " 0 R";
    }
    addObj(pagesId, "<< /Type /Pages /Kids [" + kids + "] /Count " +
                        std::to_string(pageCount) + " >>");

    for (int p = 0; p < pageCount; ++p) {
        const int pageId = firstPageId + p;
        const int contentId = firstContentId + p;
        addObj(pageId,
              "<< /Type /Page /Parent " + std::to_string(pagesId) +
                  " 0 R /MediaBox [0 0 " + std::to_string(pageWidth) + " " +
                  std::to_string(pageHeight) + "] /Contents " +
                  std::to_string(contentId) +
                  " 0 R /Resources << /Font << /F1 " +
                  std::to_string(fontRegId) + " 0 R /F2 " +
                  std::to_string(fontBoldId) + " 0 R >> >> >>");

        std::string content;
        int y = topY;
        for (const auto& u : pages[static_cast<std::size_t>(p)]) {
            switch (u.kind) {
                case Unit::Kind::Text:
                    content += pdf::renderTextLine(u.text, u.bold, u.size,
                                                   marginX, y);
                    break;
                case Unit::Kind::TableRow:
                    content += pdf::renderTableRow(u.cells, u.bold, y, colX,
                                                   colW, u.size);
                    break;
                case Unit::Kind::Rule:
                    content +=
                        pdf::renderRule(y + 3, marginX, pageWidth - marginX);
                    break;
                case Unit::Kind::Blank:
                    break;
            }
            y -= lineHeight;
        }
        const std::string footer =
            "Page " + std::to_string(p + 1) + " of " +
            std::to_string(pageCount);
        content += pdf::renderTextLine(footer, false, 8, pageWidth / 2 - 30,
                                       30);

        addObj(contentId, "<< /Length " + std::to_string(content.size()) +
                              " >>\nstream\n" + content + "endstream");
    }

    addObj(fontRegId,
          "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding "
          "/WinAnsiEncoding >>");
    addObj(fontBoldId,
          "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold "
          "/Encoding /WinAnsiEncoding >>");

    const std::size_t xrefOffset = out.size();
    const int objCount = fontBoldId + 1;
    std::string xref = "xref\n0 " + std::to_string(objCount) + "\n";
    xref += "0000000000 65535 f \n";
    for (int id = 1; id < objCount; ++id) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%010zu 00000 n \n",
                      offsets[static_cast<std::size_t>(id)]);
        xref += buf;
    }
    out += xref;
    out += "trailer\n<< /Size " + std::to_string(objCount) + " /Root " +
           std::to_string(catalogId) + " 0 R >>\n";
    out += "startxref\n" + std::to_string(xrefOffset) + "\n%%EOF\n";

    return std::vector<std::uint8_t>(out.begin(), out.end());
}

// ---------------------------------------------------------------------------
// Minimal ZIP writer (store method, no compression) for the docx container.
// ---------------------------------------------------------------------------
std::uint32_t crc32(const std::vector<std::uint8_t>& data) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::uint8_t b : data) {
        crc ^= b;
        for (int i = 0; i < 8; ++i) {
            std::uint32_t mask = (crc & 1u) ? 0xEDB88320u : 0u;
            crc = (crc >> 1) ^ mask;
        }
    }
    return ~crc;
}

void putU16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}

void putU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

std::vector<std::uint8_t> buildZip(
    const std::vector<std::pair<std::string, std::string>>& files) {
    std::vector<std::uint8_t> out;
    std::vector<std::uint8_t> central;

    for (const auto& f : files) {
        const std::string& name = f.first;
        std::vector<std::uint8_t> data(f.second.begin(), f.second.end());
        const std::uint32_t crc = crc32(data);
        const std::uint32_t size = static_cast<std::uint32_t>(data.size());

        const std::uint32_t localOffset = static_cast<std::uint32_t>(out.size());

        // Local file header.
        putU32(out, 0x04034b50u);
        putU16(out, 20);            // version needed
        putU16(out, 0);             // flags
        putU16(out, 0);             // method: store
        putU16(out, 0);             // mod time
        putU16(out, 0x21);          // mod date
        putU32(out, crc);
        putU32(out, size);          // compressed size
        putU32(out, size);          // uncompressed size
        putU16(out, static_cast<std::uint16_t>(name.size()));
        putU16(out, 0);             // extra length
        out.insert(out.end(), name.begin(), name.end());
        out.insert(out.end(), data.begin(), data.end());

        // Central directory entry.
        putU32(central, 0x02014b50u);
        putU16(central, 20);        // version made by
        putU16(central, 20);        // version needed
        putU16(central, 0);         // flags
        putU16(central, 0);         // method
        putU16(central, 0);         // mod time
        putU16(central, 0x21);      // mod date
        putU32(central, crc);
        putU32(central, size);
        putU32(central, size);
        putU16(central, static_cast<std::uint16_t>(name.size()));
        putU16(central, 0);         // extra length
        putU16(central, 0);         // comment length
        putU16(central, 0);         // disk number
        putU16(central, 0);         // internal attrs
        putU32(central, 0);         // external attrs
        putU32(central, localOffset);
        central.insert(central.end(), name.begin(), name.end());
    }

    const std::uint32_t cdOffset = static_cast<std::uint32_t>(out.size());
    out.insert(out.end(), central.begin(), central.end());

    // End of central directory.
    putU32(out, 0x06054b50u);
    putU16(out, 0);                 // disk number
    putU16(out, 0);                 // disk with cd
    putU16(out, static_cast<std::uint16_t>(files.size()));
    putU16(out, static_cast<std::uint16_t>(files.size()));
    putU32(out, static_cast<std::uint32_t>(central.size()));
    putU32(out, cdOffset);
    putU16(out, 0);                 // comment length

    return out;
}

// ---------------------------------------------------------------------------
// docx (OOXML) part builders.
// ---------------------------------------------------------------------------
namespace docx {

std::string paragraph(const std::string& text, const std::string& style) {
    const std::string styleXml =
        style.empty() ? std::string()
                      : ("<w:pPr><w:pStyle w:val=\"" + style + "\"/></w:pPr>");
    return "<w:p>" + styleXml + "<w:r><w:t xml:space=\"preserve\">" +
           xmlEscape(text) + "</w:t></w:r></w:p>";
}

std::string cell(const std::string& text, bool bold) {
    const std::string rPr = bold ? "<w:rPr><w:b/></w:rPr>" : std::string();
    return "<w:tc><w:tcPr><w:tcW w:w=\"0\" w:type=\"auto\"/></w:tcPr>"
           "<w:p><w:r>" + rPr + "<w:t xml:space=\"preserve\">" +
           xmlEscape(text) + "</w:t></w:r></w:p></w:tc>";
}

std::string row(const std::vector<std::string>& cells, bool bold) {
    std::string out = "<w:tr>";
    for (const auto& c : cells) out += cell(c, bold);
    out += "</w:tr>";
    return out;
}

std::string table(const ReportDoc& doc) {
    std::string out =
        "<w:tbl><w:tblPr><w:tblW w:w=\"0\" w:type=\"auto\"/><w:tblBorders>"
        "<w:top w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
        "<w:left w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
        "<w:bottom w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
        "<w:right w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
        "<w:insideH w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
        "<w:insideV w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
        "</w:tblBorders></w:tblPr><w:tblGrid>";
    for (std::size_t i = 0; i < doc.tableHeader.size(); ++i) {
        out += "<w:gridCol w:w=\"1600\"/>";
    }
    out += "</w:tblGrid>";
    out += row(doc.tableHeader, true);
    for (const auto& r : doc.tableRows) out += row(r, false);
    out += "</w:tbl>";
    return out;
}

std::string document(const ReportDoc& doc) {
    std::string body;
    body += paragraph(doc.title, "Title");
    for (const auto& m : doc.metaLines) body += paragraph(m, "");
    body += paragraph("Checklist Results", "Heading1");
    body += table(doc);
    if (!doc.auditLines.empty()) {
        body += paragraph("Review & Approval Audit Trail", "Heading1");
        for (const auto& a : doc.auditLines) body += paragraph(a, "");
    }
    body += "<w:sectPr><w:pgSz w:w=\"12240\" w:h=\"15840\"/>"
            "<w:pgMar w:top=\"1440\" w:right=\"1440\" w:bottom=\"1440\" "
            "w:left=\"1440\" w:header=\"720\" w:footer=\"720\" "
            "w:gutter=\"0\"/></w:sectPr>";

    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
           "<w:document "
           "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/"
           "2006/main\"><w:body>" +
           body + "</w:body></w:document>";
}

std::string contentTypes() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
           "<Types "
           "xmlns=\"http://schemas.openxmlformats.org/package/2006/"
           "content-types\">"
           "<Default Extension=\"rels\" "
           "ContentType=\"application/vnd.openxmlformats-package."
           "relationships+xml\"/>"
           "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
           "<Override PartName=\"/word/document.xml\" "
           "ContentType=\"application/vnd.openxmlformats-officedocument."
           "wordprocessingml.document.main+xml\"/>"
           "<Override PartName=\"/word/styles.xml\" "
           "ContentType=\"application/vnd.openxmlformats-officedocument."
           "wordprocessingml.styles+xml\"/>"
           "<Override PartName=\"/word/settings.xml\" "
           "ContentType=\"application/vnd.openxmlformats-officedocument."
           "wordprocessingml.settings+xml\"/>"
           "<Override PartName=\"/word/fontTable.xml\" "
           "ContentType=\"application/vnd.openxmlformats-officedocument."
           "wordprocessingml.fontTable+xml\"/>"
           "<Override PartName=\"/docProps/core.xml\" "
           "ContentType=\"application/vnd.openxmlformats-package."
           "core-properties+xml\"/>"
           "<Override PartName=\"/docProps/app.xml\" "
           "ContentType=\"application/vnd.openxmlformats-officedocument."
           "extended-properties+xml\"/>"
           "</Types>";
}

std::string rootRels() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
           "<Relationships "
           "xmlns=\"http://schemas.openxmlformats.org/package/2006/"
           "relationships\">"
           "<Relationship Id=\"rId1\" "
           "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
           "relationships/officeDocument\" Target=\"word/document.xml\"/>"
           "<Relationship Id=\"rId2\" "
           "Type=\"http://schemas.openxmlformats.org/package/2006/"
           "relationships/metadata/core-properties\" "
           "Target=\"docProps/core.xml\"/>"
           "<Relationship Id=\"rId3\" "
           "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
           "relationships/extended-properties\" "
           "Target=\"docProps/app.xml\"/>"
           "</Relationships>";
}

std::string documentRels() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
           "<Relationships "
           "xmlns=\"http://schemas.openxmlformats.org/package/2006/"
           "relationships\">"
           "<Relationship Id=\"rId1\" "
           "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
           "relationships/styles\" Target=\"styles.xml\"/>"
           "<Relationship Id=\"rId2\" "
           "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
           "relationships/settings\" Target=\"settings.xml\"/>"
           "<Relationship Id=\"rId3\" "
           "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
           "relationships/fontTable\" Target=\"fontTable.xml\"/>"
           "</Relationships>";
}

std::string styles() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
           "<w:styles "
           "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/"
           "2006/main\">"
           "<w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:ascii=\"Calibri\" "
           "w:hAnsi=\"Calibri\"/><w:sz w:val=\"22\"/></w:rPr></w:rPrDefault>"
           "</w:docDefaults>"
           "<w:style w:type=\"paragraph\" w:default=\"1\" "
           "w:styleId=\"Normal\"><w:name w:val=\"Normal\"/></w:style>"
           "<w:style w:type=\"paragraph\" w:styleId=\"Title\">"
           "<w:name w:val=\"Title\"/><w:basedOn w:val=\"Normal\"/>"
           "<w:pPr><w:spacing w:after=\"240\"/></w:pPr>"
           "<w:rPr><w:b/><w:sz w:val=\"36\"/></w:rPr></w:style>"
           "<w:style w:type=\"paragraph\" w:styleId=\"Heading1\">"
           "<w:name w:val=\"heading 1\"/><w:basedOn w:val=\"Normal\"/>"
           "<w:pPr><w:spacing w:before=\"240\" w:after=\"120\"/></w:pPr>"
           "<w:rPr><w:b/><w:sz w:val=\"28\"/></w:rPr></w:style>"
           "</w:styles>";
}

std::string settings() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
           "<w:settings "
           "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/"
           "2006/main\"><w:defaultTabStop w:val=\"720\"/></w:settings>";
}

std::string fontTable() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
           "<w:fonts "
           "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/"
           "2006/main\"><w:font w:name=\"Calibri\">"
           "<w:family w:val=\"swiss\"/></w:font></w:fonts>";
}

std::string coreProps(const std::string& title, const std::string& ts) {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
           "<cp:coreProperties "
           "xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/"
           "metadata/core-properties\" "
           "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
           "xmlns:dcterms=\"http://purl.org/dc/terms/\" "
           "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
           "<dc:title>" + xmlEscape(title) + "</dc:title>"
           "<dc:creator>Lodestar AssureCheck</dc:creator>"
           "<cp:lastModifiedBy>Lodestar AssureCheck</cp:lastModifiedBy>"
           "<dcterms:created xsi:type=\"dcterms:W3CDTF\">" + ts +
           "</dcterms:created>"
           "<dcterms:modified xsi:type=\"dcterms:W3CDTF\">" + ts +
           "</dcterms:modified>"
           "</cp:coreProperties>";
}

std::string appProps() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
           "<Properties "
           "xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/"
           "extended-properties\">"
           "<Application>Lodestar AssureCheck</Application></Properties>";
}

std::vector<std::pair<std::string, std::string>> parts(const ReportDoc& doc) {
    const std::string ts = common::nowIso();
    return {
        {"[Content_Types].xml", contentTypes()},
        {"_rels/.rels", rootRels()},
        {"word/document.xml", document(doc)},
        {"word/_rels/document.xml.rels", documentRels()},
        {"word/styles.xml", styles()},
        {"word/settings.xml", settings()},
        {"word/fontTable.xml", fontTable()},
        {"docProps/core.xml", coreProps(doc.title, ts)},
        {"docProps/app.xml", appProps()},
    };
}

}  // namespace docx

// ---------------------------------------------------------------------------
// ReQIF (XML) writer.
// ---------------------------------------------------------------------------
std::string buildReqif(const std::vector<tracelink::Entity>& requirements,
                       const std::vector<tracelink::Link>& links) {
    std::string xml;
    xml += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml += "<REQ-IF xmlns=\"http://www.omg.org/spec/ReqIF/20110401/reqif.xsd\">\n";
    xml += "  <THE-HEADER><REQ-IF-HEADER IDENTIFIER=\"lodestar-reqif\">"
           "<TITLE>Lodestar ReQIF Export</TITLE></REQ-IF-HEADER></THE-HEADER>\n";
    xml += "  <CORE-CONTENT><REQ-IF-CONTENT>\n";

    // DATATYPES first -- everything below references a datatype by id, and a
    // real ReQIF consumer (DOORS/Polarion/Codebeamer) validates that the
    // reference resolves.
    xml += "    <DATATYPES>\n";
    xml += "      <DATATYPE-DEFINITION-STRING IDENTIFIER=\"STRING-DATATYPE\" "
           "LONG-NAME=\"String\" MAX-LENGTH=\"4000\"/>\n";
    xml += "    </DATATYPES>\n";

    // SPEC-TYPES defines REQ-TYPE and TRACE-TYPE so the SPEC-OBJECT-TYPE-REF
    // / SPEC-RELATION-TYPE-REF below aren't dangling (S3 Phase 4 fix; the gap
    // this closes is documented in PLAN.md, Phase 4 brief item 3).
    xml += "    <SPEC-TYPES>\n";
    xml += "      <SPEC-OBJECT-TYPE IDENTIFIER=\"REQ-TYPE\" "
           "LONG-NAME=\"Requirement\">\n";
    xml += "        <SPEC-ATTRIBUTES>\n";
    xml += "          <ATTRIBUTE-DEFINITION-STRING IDENTIFIER=\"REQ-TYPE-NAME\" "
           "LONG-NAME=\"Name\">\n";
    xml += "            <TYPE><DATATYPE-DEFINITION-STRING-REF>STRING-DATATYPE"
           "</DATATYPE-DEFINITION-STRING-REF></TYPE>\n";
    xml += "          </ATTRIBUTE-DEFINITION-STRING>\n";
    xml += "          <ATTRIBUTE-DEFINITION-STRING IDENTIFIER=\"REQ-TYPE-TEXT\" "
           "LONG-NAME=\"Text\">\n";
    xml += "            <TYPE><DATATYPE-DEFINITION-STRING-REF>STRING-DATATYPE"
           "</DATATYPE-DEFINITION-STRING-REF></TYPE>\n";
    xml += "          </ATTRIBUTE-DEFINITION-STRING>\n";
    xml += "        </SPEC-ATTRIBUTES>\n";
    xml += "      </SPEC-OBJECT-TYPE>\n";
    xml += "      <SPEC-RELATION-TYPE IDENTIFIER=\"TRACE-TYPE\" "
           "LONG-NAME=\"Trace\"/>\n";
    xml += "    </SPEC-TYPES>\n";

    xml += "    <SPEC-OBJECTS>\n";
    for (const auto& r : requirements) {
        const std::string id =
            r.externalId.empty() ? r.id : r.externalId;
        xml += "      <SPEC-OBJECT IDENTIFIER=\"" + xmlEscape(id) + "\">\n";
        xml += "        <VALUES>\n";
        xml += "          <ATTRIBUTE-VALUE-STRING THE-VALUE=\"" +
               xmlEscape(r.name) + "\">\n";
        xml += "            <DEFINITION><ATTRIBUTE-DEFINITION-STRING-REF>"
               "REQ-TYPE-NAME</ATTRIBUTE-DEFINITION-STRING-REF></DEFINITION>\n";
        xml += "          </ATTRIBUTE-VALUE-STRING>\n";
        xml += "          <ATTRIBUTE-VALUE-STRING THE-VALUE=\"" +
               xmlEscape(r.text) + "\">\n";
        xml += "            <DEFINITION><ATTRIBUTE-DEFINITION-STRING-REF>"
               "REQ-TYPE-TEXT</ATTRIBUTE-DEFINITION-STRING-REF></DEFINITION>\n";
        xml += "          </ATTRIBUTE-VALUE-STRING>\n";
        xml += "        </VALUES>\n";
        xml += "        <TYPE><SPEC-OBJECT-TYPE-REF>REQ-TYPE</SPEC-OBJECT-TYPE-REF>"
               "</TYPE>\n";
        xml += "      </SPEC-OBJECT>\n";
    }
    xml += "    </SPEC-OBJECTS>\n";
    xml += "    <SPEC-RELATIONS>\n";
    for (const auto& l : links) {
        xml += "      <SPEC-RELATION IDENTIFIER=\"" + xmlEscape(l.id) + "\">\n";
        xml += "        <SOURCE><SPEC-OBJECT-REF>" + xmlEscape(l.sourceId) +
               "</SPEC-OBJECT-REF></SOURCE>\n";
        xml += "        <TARGET><SPEC-OBJECT-REF>" + xmlEscape(l.targetId) +
               "</SPEC-OBJECT-REF></TARGET>\n";
        xml += "        <TYPE><SPEC-RELATION-TYPE-REF>TRACE-TYPE</SPEC-RELATION-TYPE-REF>"
               "</TYPE>\n";
        xml += "      </SPEC-RELATION>\n";
    }
    xml += "    </SPEC-RELATIONS>\n";
    xml += "  </REQ-IF-CONTENT></CORE-CONTENT>\n";
    xml += "  <TOOL-EXTENSIONS/>\n";
    xml += "</REQ-IF>\n";
    return xml;
}

}  // namespace

CertReportService::CertReportService(persistence::Database& db)
    : db_(db), workflow_(db) {}

std::vector<std::vector<AuditEntry>> CertReportService::gatherAuditByRow(
    const ComplianceReport& report) {
    std::vector<std::vector<AuditEntry>> out;
    out.reserve(report.rows.size());
    for (const auto& row : report.rows) {
        if (row.resultId.empty()) {
            out.emplace_back();
            continue;
        }
        auto log = workflow_.auditLog(row.resultId);
        out.push_back(log.isOk() ? log.value() : std::vector<AuditEntry>{});
    }
    return out;
}

common::Result<std::vector<std::uint8_t>> CertReportService::exportPdf(
    const ComplianceReport& report) {
    const ReportDoc doc = buildReportDoc(report, gatherAuditByRow(report));
    return common::Result<std::vector<std::uint8_t>>::ok(buildPdf(doc));
}

common::Result<std::vector<std::uint8_t>> CertReportService::exportWord(
    const ComplianceReport& report) {
    const ReportDoc doc = buildReportDoc(report, gatherAuditByRow(report));
    return common::Result<std::vector<std::uint8_t>>::ok(
        buildZip(docx::parts(doc)));
}

common::Result<std::vector<std::uint8_t>> CertReportService::exportReqif(
    const std::vector<tracelink::Entity>& requirements,
    const std::vector<tracelink::Link>& links) {
    const std::string xml = buildReqif(requirements, links);
    return common::Result<std::vector<std::uint8_t>>::ok(
        std::vector<std::uint8_t>(xml.begin(), xml.end()));
}

common::Result<std::vector<std::string>>
CertReportService::verifiedRequirementsForTestCase(
    const std::string& testCaseId) {
    sqlite3* db = db_.handle();
    if (db == nullptr) {
        return common::Result<std::vector<std::string>>::err(
            "database not open");
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT target_id FROM trace_links "
        "WHERE source_type='test_case' AND source_id=? "
        "AND target_type='requirement' AND relation='verifies' "
        "AND status='Active' ORDER BY target_id;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::vector<std::string>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db)));
    }
    sqlite3_bind_text(stmt, 1, testCaseId.c_str(),
                      static_cast<int>(testCaseId.size()), SQLITE_TRANSIENT);

    std::vector<std::string> out;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* t = sqlite3_column_text(stmt, 0);
        out.push_back(t ? reinterpret_cast<const char*>(t) : std::string());
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<std::string>>::ok(std::move(out));
}

common::Result<std::vector<std::string>>
CertReportService::traceResultToRequirements(const std::string& resultId) {
    return verifiedRequirementsForTestCase(resultId);
}

}  // namespace lodestar::assurecheck
