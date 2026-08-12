// core/assurecheck/CertReportService.cpp
// S2 Phase 8: certification-ready reporting + traceability implementation.

#include "core/assurecheck/CertReportService.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <sqlite3.h>

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

// ---------------------------------------------------------------------------
// Minimal but valid PDF writer (single page, Helvetica body text).
// ---------------------------------------------------------------------------
std::vector<std::uint8_t> buildPdf(const std::string& body) {
    std::string pdf;
    pdf += "%PDF-1.4\n";

    std::vector<size_t> offsets;
    auto addObj = [&](const std::string& obj) {
        offsets.push_back(pdf.size());
        pdf += obj;
    };

    addObj("1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
    addObj("2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");
    addObj("3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
           "/Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>\nendobj\n");

    std::string content =
        "BT /F1 12 Tf 50 750 Td (" + pdfEscape(body) + ") Tj ET\n";
    addObj("4 0 obj\n<< /Length " + std::to_string(content.size()) +
           " >>\nstream\n" + content + "endstream\nendobj\n");

    addObj("5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\n"
           "endobj\n");

    const size_t xrefOffset = pdf.size();
    std::string xref = "xref\n0 " + std::to_string(offsets.size() + 1) + "\n";
    xref += "0000000000 65535 f \n";
    for (size_t off : offsets) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%010zu 00000 n \n", off);
        xref += buf;
    }
    pdf += xref;
    pdf += "trailer\n<< /Size " + std::to_string(offsets.size() + 1) +
           " /Root 1 0 R >>\n";
    pdf += "startxref\n" + std::to_string(xrefOffset) + "\n%%EOF\n";

    return std::vector<std::uint8_t>(pdf.begin(), pdf.end());
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
    std::vector<std::uint32_t> localOffsets;

    for (const auto& f : files) {
        const std::string& name = f.first;
        std::vector<std::uint8_t> data(f.second.begin(), f.second.end());
        const std::uint32_t crc = crc32(data);
        const std::uint32_t size = static_cast<std::uint32_t>(data.size());

        const std::uint32_t localOffset = static_cast<std::uint32_t>(out.size());
        localOffsets.push_back(localOffset);

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
    xml += "    <SPEC-OBJECTS>\n";
    for (const auto& r : requirements) {
        const std::string id =
            r.externalId.empty() ? r.id : r.externalId;
        xml += "      <SPEC-OBJECT IDENTIFIER=\"" + xmlEscape(id) + "\">\n";
        xml += "        <VALUES><ATTRIBUTE-VALUE-STRING THE-VALUE=\"" +
               xmlEscape(r.name) + "\"/></VALUES>\n";
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
    xml += "</REQ-IF>\n";
    return xml;
}

// Renders a compliance report as a plain-text body for PDF / Word.
std::string reportBody(const ComplianceReport& report) {
    std::string body;
    body += "Compliance Report\n";
    body += "Standard: " + report.standardCode + " - " + report.standardName + "\n";
    body += "Project DAL: " + report.dalLevel + "\n";
    body += "Coverage: " + std::to_string(report.coverage.percent) + "%\n";
    body += "Pass: " + std::to_string(report.coverage.pass) +
            "  Fail: " + std::to_string(report.coverage.fail) +
            "  NA: " + std::to_string(report.coverage.na) +
            "  Warning: " + std::to_string(report.coverage.warning) + "\n\n";
    for (const auto& row : report.rows) {
        body += row.itemCode + " | " + row.status + " | " + row.dalLevel +
                " | " + row.objective + "\n";
    }
    return body;
}

}  // namespace

CertReportService::CertReportService(persistence::Database& db) : db_(db) {}

common::Result<std::vector<std::uint8_t>> CertReportService::exportPdf(
    const ComplianceReport& report) {
    return common::Result<std::vector<std::uint8_t>>::ok(
        buildPdf(reportBody(report)));
}

common::Result<std::vector<std::uint8_t>> CertReportService::exportWord(
    const ComplianceReport& report) {
    const std::string body = reportBody(report);

    std::string contentTypes =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
        "</Types>";

    std::string rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
        "</Relationships>";

    std::string document =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body>";
    std::string line;
    for (char c : body) {
        if (c == '\n') {
            document += "<w:p><w:r><w:t xml:space=\"preserve\">" +
                        xmlEscape(line) + "</w:t></w:r></w:p>";
            line.clear();
        } else {
            line += c;
        }
    }
    if (!line.empty()) {
        document += "<w:p><w:r><w:t xml:space=\"preserve\">" +
                    xmlEscape(line) + "</w:t></w:r></w:p>";
    }
    document += "</w:body></w:document>";

    std::vector<std::pair<std::string, std::string>> files = {
        {"[Content_Types].xml", contentTypes},
        {"_rels/.rels", rels},
        {"word/document.xml", document}};

    return common::Result<std::vector<std::uint8_t>>::ok(buildZip(files));
}

common::Result<std::vector<std::uint8_t>> CertReportService::exportReqif(
    const std::vector<tracelink::Entity>& requirements,
    const std::vector<tracelink::Link>& links) {
    const std::string xml = buildReqif(requirements, links);
    return common::Result<std::vector<std::uint8_t>>::ok(
        std::vector<std::uint8_t>(xml.begin(), xml.end()));
}

common::Result<std::vector<std::string>> CertReportService::
    traceResultToRequirements(const std::string& resultId) {
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
    sqlite3_bind_text(stmt, 1, resultId.c_str(),
                      static_cast<int>(resultId.size()), SQLITE_TRANSIENT);

    std::vector<std::string> out;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* t = sqlite3_column_text(stmt, 0);
        out.push_back(t ? reinterpret_cast<const char*>(t) : std::string());
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<std::string>>::ok(std::move(out));
}

}  // namespace lodestar::assurecheck
