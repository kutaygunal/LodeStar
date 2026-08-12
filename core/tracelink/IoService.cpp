#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// core/tracelink/IoService.cpp
// WP-5 import/export: CSV (trace matrix, entities, links), auditor-ready HTML
// report, and ReqIF XML. Imports are non-destructive and always record an
// import_batches row plus per-record import_log rows.

#include "core/tracelink/IoService.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <map>
#include <sstream>
#include <utility>

#include <sqlite3.h>

#include "core/common/Uuid.h"
#include "core/tracelink/GraphEngine.h"
#include "core/tracelink/StateMachine.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"

namespace lodestar::tracelink {

using lodestar::common::newUuid;

namespace {

std::string now() {
    char buf[32];
    const auto t = std::time(nullptr);
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return buf;
}

// ---------------------------------------------------------------------------
// Minimal SQL helpers (same pattern as BaselineService.cpp).
// ---------------------------------------------------------------------------
void bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()),
                      SQLITE_TRANSIENT);
}

common::Result<void> exec(sqlite3* db, const std::string& sql,
                          const std::vector<std::string>& params = {}) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<void>::err("prepare failed: " +
                                         std::string(sqlite3_errmsg(db)));
    }
    for (size_t i = 0; i < params.size(); ++i) {
        bindText(stmt, static_cast<int>(i + 1), params[i]);
    }
    int rc = sqlite3_step(stmt);
    std::string msg = sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err("step failed: " + msg);
    }
    return common::Result<void>::ok();
}

// ---------------------------------------------------------------------------
// CSV helpers. Fields are comma-free (no quoting, per the WP-5 contract).
// ---------------------------------------------------------------------------
std::vector<std::string> splitLines(const std::string& content) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : content) {
        if (c == '\n') {
            if (!cur.empty() && cur.back() == '\r') cur.pop_back();
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else if (c == '\r') {
            // ignore bare CR
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::vector<std::string> splitCsv(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == ',') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

std::string joinComma(const std::vector<std::string>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) s += ",";
        s += v[i];
    }
    return s;
}

// ---------------------------------------------------------------------------
// ReqIF XML escape / unescape helpers.
// ---------------------------------------------------------------------------
std::string xmlEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;"; break;
            case '>':  out += "&gt;"; break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out += c;
        }
    }
    return out;
}

std::string xmlUnescape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '&') {
            if (s.compare(i, 5, "&amp;") == 0) { out += '&'; i += 4; }
            else if (s.compare(i, 4, "&lt;") == 0) { out += '<'; i += 3; }
            else if (s.compare(i, 4, "&gt;") == 0) { out += '>'; i += 3; }
            else if (s.compare(i, 6, "&quot;") == 0) { out += '"'; i += 5; }
            else if (s.compare(i, 6, "&apos;") == 0) { out += '\''; i += 5; }
            else out += s[i];
        } else {
            out += s[i];
        }
    }
    return out;
}

// Extracts the inner text of a tag like <TAG>...</TAG> (returns "" if absent).
std::string extractTag(const std::string& xml, const std::string& tag) {
    std::string open = "<" + tag + ">";
    std::string close = "</" + tag + ">";
    auto b = xml.find(open);
    if (b == std::string::npos) return "";
    b += open.size();
    auto e = xml.find(close, b);
    if (e == std::string::npos) return "";
    return xmlUnescape(xml.substr(b, e - b));
}

// Splits content into consecutive <open>...</close> blocks.
std::vector<std::string> extractBlocks(const std::string& xml,
                                       const std::string& open,
                                       const std::string& close) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (true) {
        auto b = xml.find(open, pos);
        if (b == std::string::npos) break;
        auto e = xml.find(close, b + open.size());
        if (e == std::string::npos) break;
        out.push_back(xml.substr(b + open.size(), e - (b + open.size())));
        pos = e + close.size();
    }
    return out;
}

// The four core + three auxiliary entity types, in a stable export order.
const std::vector<EntityType>& allEntityTypes() {
    static const std::vector<EntityType> kTypes = {
        EntityType::Requirement, EntityType::Design,     EntityType::Interface,
        EntityType::TestCase,    EntityType::Hazard,     EntityType::Decision,
        EntityType::Assumption};
    return kTypes;
}

// internal id -> external id map for one entity type.
std::map<std::string, std::string> idToExt(TraceLinkService& svc, EntityType t) {
    std::map<std::string, std::string> m;
    auto list = svc.listEntities(t, EntityFilter{});
    if (list.isOk()) {
        for (const auto& e : list.value()) {
            if (e.status == "Obsolete") continue;
            m[e.id] = e.externalId;
        }
    }
    return m;
}

}  // namespace

// ---------------------------------------------------------------------------
// IoService.
// ---------------------------------------------------------------------------
IoService::IoService(persistence::Database& db) : db_(db) {}

std::string IoService::resolveEntityId(const std::string& type,
                                       const std::string& externalId) {
    TraceLinkService svc(db_);
    auto t = entityTypeFromString(type);
    if (!t) return "";
    auto list = svc.listEntities(*t, EntityFilter{});
    if (!list.isOk()) return "";
    for (const auto& e : list.value()) {
        if (e.externalId == externalId) return e.id;
    }
    return "";
}

// ---------------------------------------------------------------------------
// Export: entities CSV (one line per active entity, all types; no header).
// ---------------------------------------------------------------------------
common::Result<std::string> IoService::entitiesCsv() {
    TraceLinkService svc(db_);
    std::string out;
    for (auto t : allEntityTypes()) {
        auto list = svc.listEntities(t, EntityFilter{});
        if (list.failed()) return common::Result<std::string>::err(list.error());
        for (const auto& e : list.value()) {
            if (e.status == "Obsolete") continue;
            std::vector<std::string> f = {
                "entity", toString(e.type), e.externalId, e.name, e.text, e.status};
            out += joinComma(f) + "\n";
        }
    }
    return common::Result<std::string>::ok(std::move(out));
}

// ---------------------------------------------------------------------------
// Export: links CSV (one line per Active link, keyed by external ids).
// ---------------------------------------------------------------------------
common::Result<std::string> IoService::linksCsv() {
    TraceLinkService svc(db_);
    std::map<EntityType, std::map<std::string, std::string>> extByType;
    for (auto t : allEntityTypes()) extByType[t] = idToExt(svc, t);

    auto links = svc.allLinks();
    if (links.failed()) return common::Result<std::string>::err(links.error());

    std::string out;
    for (const auto& l : links.value()) {
        if (l.status == "Superseded") continue;
        const std::string srcExt = extByType[l.sourceType][l.sourceId];
        const std::string tgtExt = extByType[l.targetType][l.targetId];
        std::vector<std::string> f = {
            "link", toString(l.sourceType), srcExt, l.relation,
            toString(l.targetType), tgtExt};
        out += joinComma(f) + "\n";
    }
    return common::Result<std::string>::ok(std::move(out));
}

// ---------------------------------------------------------------------------
// Export: trace matrix CSV (reuses GraphEngine::traceMatrix).
// ---------------------------------------------------------------------------
common::Result<std::string> IoService::matrixCsv() {
    GraphEngine ge(db_);
    auto m = ge.traceMatrix();
    if (m.failed()) return common::Result<std::string>::err(m.error());

    // Map column internal ids to external ids for readability.
    std::map<std::string, std::string> colExt;
    {
        TraceLinkService svc(db_);
        auto designExt = idToExt(svc, EntityType::Design);
        auto testExt = idToExt(svc, EntityType::TestCase);
        for (const auto& [id, ext] : designExt) colExt[id] = ext;
        for (const auto& [id, ext] : testExt) colExt[id] = ext;
    }

    std::string out;
    std::vector<std::string> header = {"requirement"};
    for (const auto& cid : m.value().columnIds) header.push_back(colExt[cid]);
    out += joinComma(header) + "\n";

    for (const auto& row : m.value().rows) {
        std::vector<std::string> f = {row.requirementExternalId};
        for (const auto& c : row.cells) f.push_back(c.relation);
        out += joinComma(f) + "\n";
    }
    return common::Result<std::string>::ok(std::move(out));
}

// ---------------------------------------------------------------------------
// Export: auditor-ready HTML report.
// ---------------------------------------------------------------------------
common::Result<std::string> IoService::matrixHtml() {
    GraphEngine ge(db_);
    auto m = ge.traceMatrix();
    if (m.failed()) return common::Result<std::string>::err(m.error());

    std::map<std::string, std::string> colExt;
    {
        TraceLinkService svc(db_);
        auto designExt = idToExt(svc, EntityType::Design);
        auto testExt = idToExt(svc, EntityType::TestCase);
        for (const auto& [id, ext] : designExt) colExt[id] = ext;
        for (const auto& [id, ext] : testExt) colExt[id] = ext;
    }

    std::string html;
    html += "<!DOCTYPE html>\n";
    html += "<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n";
    html += "<title>Lodestar Trace Matrix</title>\n";
    html += "<style>body{font-family:sans-serif;margin:2em}table{border-collapse:"
            "collapse}td,th{border:1px solid #999;padding:4px 8px;text-align:center}"
            "th{background:#eee}td.req{text-align:left;font-weight:bold}</style>\n";
    html += "</head>\n<body>\n";
    html += "<h1>Lodestar Requirement Trace Matrix</h1>\n";
    html += "<p>Generated: " + now() + "</p>\n";
    html += "<p>Requirements: " + std::to_string(m.value().rows.size()) +
            " | Columns: " + std::to_string(m.value().columnIds.size()) + "</p>\n";
    html += "<table>\n<tr><th>Requirement</th>";
    for (const auto& cid : m.value().columnIds) {
        html += "<th>" + xmlEscape(colExt[cid]) + "</th>";
    }
    html += "</tr>\n";
    for (const auto& row : m.value().rows) {
        html += "<tr><td class=\"req\">" + xmlEscape(row.requirementExternalId) +
                "</td>";
        for (const auto& c : row.cells) {
            html += "<td>" + xmlEscape(c.relation) + "</td>";
        }
        html += "</tr>\n";
    }
    html += "</table>\n</body>\n</html>\n";
    return common::Result<std::string>::ok(std::move(html));
}

// ---------------------------------------------------------------------------
// Export: ReqIF XML (entities as SPEC-OBJECT, links as SPEC-RELATION).
// ---------------------------------------------------------------------------
common::Result<std::string> IoService::reqif() {
    TraceLinkService svc(db_);
    std::map<EntityType, std::map<std::string, std::string>> extByType;
    for (auto t : allEntityTypes()) extByType[t] = idToExt(svc, t);

    std::string xml;
    xml += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml += "<REQ-IF xmlns=\"http://www.omg.org/spec/ReqIF/20110401/reqif.xsd\">\n";
    xml += "  <THE-HEADER><REQ-IF-HEADER IDENTIFIER=\"lodestar\"><TITLE>Lodestar "
           "TraceLink Export</TITLE></REQ-IF-HEADER></THE-HEADER>\n";
    xml += "  <CORE-CONTENT><REQ-IF-CONTENT>\n";
    xml += "    <SPEC-OBJECTS>\n";

    // Entities.
    for (auto t : allEntityTypes()) {
        auto list = svc.listEntities(t, EntityFilter{});
        if (list.failed()) return common::Result<std::string>::err(list.error());
        for (const auto& e : list.value()) {
            if (e.status == "Obsolete") continue;
            xml += "      <SPEC-OBJECT>\n";
            xml += "        <TYPE>" + toString(e.type) + "</TYPE>\n";
            xml += "        <EXTERNAL-ID>" + xmlEscape(e.externalId) + "</EXTERNAL-ID>\n";
            xml += "        <NAME>" + xmlEscape(e.name) + "</NAME>\n";
            xml += "        <TEXT>" + xmlEscape(e.text) + "</TEXT>\n";
            xml += "        <STATUS>" + xmlEscape(e.status) + "</STATUS>\n";
            xml += "      </SPEC-OBJECT>\n";
        }
    }

    // Links.
    auto links = svc.allLinks();
    if (links.failed()) return common::Result<std::string>::err(links.error());
    xml += "    </SPEC-OBJECTS>\n    <SPEC-RELATIONS>\n";
    int n = 0;
    for (const auto& l : links.value()) {
        if (l.status == "Superseded") continue;
        xml += "      <SPEC-RELATION>\n";
        xml += "        <SOURCE-TYPE>" + toString(l.sourceType) + "</SOURCE-TYPE>\n";
        xml += "        <SOURCE-ID>" + xmlEscape(extByType[l.sourceType][l.sourceId]) +
               "</SOURCE-ID>\n";
        xml += "        <RELATION>" + xmlEscape(l.relation) + "</RELATION>\n";
        xml += "        <TARGET-TYPE>" + toString(l.targetType) + "</TARGET-TYPE>\n";
        xml += "        <TARGET-ID>" + xmlEscape(extByType[l.targetType][l.targetId]) +
               "</TARGET-ID>\n";
        xml += "      </SPEC-RELATION>\n";
        ++n;
    }
    xml += "    </SPEC-RELATIONS>\n";
    xml += "  </REQ-IF-CONTENT></CORE-CONTENT>\n";
    xml += "</REQ-IF>\n";
    (void)n;
    return common::Result<std::string>::ok(std::move(xml));
}

// ---------------------------------------------------------------------------
// Import: write the batch + log rows inside a transaction.
// ---------------------------------------------------------------------------
common::Result<void> IoService::persistBatch(const std::string& format,
                                             const ImportReport& report,
                                             const std::vector<ImportLogEntry>& log) {
    std::string dbStatus = report.errors > 0 ? "Partial" : "Success";
    std::string summary = "imported " + std::to_string(report.imported) + " record(s), " +
                          std::to_string(report.errors) + " error(s)";

    auto begin = db_.execute("BEGIN;");
    if (begin.failed()) return common::Result<void>::err("BEGIN failed: " + begin.error());

    auto insBatch = exec(db_.handle(),
                         "INSERT INTO import_batches (id, format, filename, imported_by, "
                         "imported_at, status, result_summary) VALUES (?,?,?,?,?,?,?);",
                         {report.batchId, format, "", "", now(), dbStatus, summary});
    if (insBatch.failed()) {
        db_.execute("ROLLBACK;");
        return common::Result<void>::err(insBatch.error());
    }
    for (const auto& e : log) {
        auto insLog = exec(db_.handle(),
                           "INSERT INTO import_log (id, batch_id, line, severity, message) "
                           "VALUES (?,?,?,?,?);",
                           {newUuid(), report.batchId, std::to_string(e.line), e.severity,
                            e.message});
        if (insLog.failed()) {
            db_.execute("ROLLBACK;");
            return common::Result<void>::err(insLog.error());
        }
    }
    auto commit = db_.execute("COMMIT;");
    if (commit.failed()) {
        db_.execute("ROLLBACK;");
        return common::Result<void>::err("COMMIT failed: " + commit.error());
    }
    return common::Result<void>::ok();
}

// ---------------------------------------------------------------------------
// Import: CSV (non-destructive). Two passes: entities first, then links.
// ---------------------------------------------------------------------------
common::Result<ImportReport> IoService::importCsv(const std::string& content) {
    TraceLinkService svc(db_);
    ImportReport report;
    report.batchId = newUuid();
    report.status = "ok";
    std::vector<ImportLogEntry> log;

    auto recordLog = [&](int line, const std::string& sev, const std::string& msg) {
        ImportLogEntry e;
        e.line = line;
        e.severity = sev;
        e.message = msg;
        log.push_back(std::move(e));
    };

    // Parse all non-empty lines into records.
    struct Rec {
        bool entity;
        std::vector<std::string> f;
        int line;
    };
    std::vector<Rec> recs;
    auto lines = splitLines(content);
    int lineNum = 0;
    for (const auto& ln : lines) {
        ++lineNum;
        auto f = splitCsv(ln);
        if (f.empty()) continue;
        Rec r;
        r.line = lineNum;
        r.f = f;
        if (f[0] == "entity") {
            r.entity = true;
        } else if (f[0] == "link") {
            r.entity = false;
        } else {
            continue;  // unknown line kind, ignore
        }
        recs.push_back(std::move(r));
    }

    // Pass 1: entities.
    for (const auto& r : recs) {
        if (!r.entity) continue;
        if (r.f.size() < 6) {
            recordLog(r.line, "error", "malformed entity record");
            report.errors++;
            continue;
        }
        auto typeOpt = entityTypeFromString(r.f[1]);
        if (!typeOpt) {
            recordLog(r.line, "error", "invalid entity type '" + r.f[1] + "'");
            report.errors++;
            continue;
        }
        if (!isValidStatus(*typeOpt, r.f[5])) {
            recordLog(r.line, "error", "invalid status '" + r.f[5] + "'");
            report.errors++;
            continue;
        }
        Entity e;
        e.type = *typeOpt;
        e.externalId = r.f[2];
        e.name = r.f[3];
        e.text = r.f[4];
        e.status = r.f[5];
        auto res = svc.addEntity(e);
        if (res.failed()) {
            recordLog(r.line, "error", "entity import failed: " + res.error());
            report.errors++;
            continue;
        }
        report.imported++;
        recordLog(r.line, "info", "imported " + toString(*typeOpt) + " " + r.f[2]);
    }

    // Pass 2: links (after entities exist).
    for (const auto& r : recs) {
        if (r.entity) continue;
        if (r.f.size() < 6) {
            recordLog(r.line, "error", "malformed link record");
            report.errors++;
            continue;
        }
        auto srcType = entityTypeFromString(r.f[1]);
        auto tgtType = entityTypeFromString(r.f[4]);
        if (!srcType || !tgtType) {
            recordLog(r.line, "error", "invalid link type in record");
            report.errors++;
            continue;
        }
        std::string srcId = resolveEntityId(toString(*srcType), r.f[2]);
        if (srcId.empty()) {
            recordLog(r.line, "error", "source entity not found: " + r.f[2]);
            report.errors++;
            continue;
        }
        std::string tgtId = resolveEntityId(toString(*tgtType), r.f[5]);
        if (tgtId.empty()) {
            recordLog(r.line, "error", "target entity not found: " + r.f[5]);
            report.errors++;
            continue;
        }
        Link l;
        l.sourceType = *srcType;
        l.sourceId = srcId;
        l.targetType = *tgtType;
        l.targetId = tgtId;
        l.relation = r.f[3];
        auto res = svc.addLink(l);
        if (res.failed()) {
            recordLog(r.line, "error", "link import failed: " + res.error());
            report.errors++;
            continue;
        }
        report.imported++;
        recordLog(r.line, "info",
                  "imported link " + r.f[2] + " -" + r.f[3] + "-> " + r.f[5]);
    }

    if (report.errors > 0) report.status = "partial";

    auto persist = persistBatch("csv", report, log);
    if (persist.failed()) return common::Result<ImportReport>::err(persist.error());

    report.log = std::move(log);
    return common::Result<ImportReport>::ok(std::move(report));
}

// ---------------------------------------------------------------------------
// Import: ReqIF XML (non-destructive).
// ---------------------------------------------------------------------------
common::Result<ImportReport> IoService::importReqif(const std::string& content) {
    TraceLinkService svc(db_);
    ImportReport report;
    report.batchId = newUuid();
    report.status = "ok";
    std::vector<ImportLogEntry> log;

    auto recordLog = [&](int line, const std::string& sev, const std::string& msg) {
        ImportLogEntry e;
        e.line = line;
        e.severity = sev;
        e.message = msg;
        log.push_back(std::move(e));
    };

    // Parse entities.
    struct ParsedEntity {
        std::string type, extId, name, text, status;
    };
    std::vector<ParsedEntity> entities;
    auto objBlocks = extractBlocks(content, "<SPEC-OBJECT>", "</SPEC-OBJECT>");
    for (const auto& b : objBlocks) {
        ParsedEntity pe;
        pe.type = extractTag(b, "TYPE");
        pe.extId = extractTag(b, "EXTERNAL-ID");
        pe.name = extractTag(b, "NAME");
        pe.text = extractTag(b, "TEXT");
        pe.status = extractTag(b, "STATUS");
        entities.push_back(std::move(pe));
    }

    // Parse links.
    struct ParsedLink {
        std::string srcType, srcId, relation, tgtType, tgtId;
    };
    std::vector<ParsedLink> links;
    auto relBlocks = extractBlocks(content, "<SPEC-RELATION>", "</SPEC-RELATION>");
    for (const auto& b : relBlocks) {
        ParsedLink pl;
        pl.srcType = extractTag(b, "SOURCE-TYPE");
        pl.srcId = extractTag(b, "SOURCE-ID");
        pl.relation = extractTag(b, "RELATION");
        pl.tgtType = extractTag(b, "TARGET-TYPE");
        pl.tgtId = extractTag(b, "TARGET-ID");
        links.push_back(std::move(pl));
    }

    // Entities first.
    int recNum = 0;
    for (const auto& pe : entities) {
        ++recNum;
        auto typeOpt = entityTypeFromString(pe.type);
        if (!typeOpt) {
            recordLog(recNum, "error", "invalid entity type '" + pe.type + "'");
            report.errors++;
            continue;
        }
        if (!isValidStatus(*typeOpt, pe.status)) {
            recordLog(recNum, "error", "invalid status '" + pe.status + "'");
            report.errors++;
            continue;
        }
        Entity e;
        e.type = *typeOpt;
        e.externalId = pe.extId;
        e.name = pe.name;
        e.text = pe.text;
        e.status = pe.status;
        auto res = svc.addEntity(e);
        if (res.failed()) {
            recordLog(recNum, "error", "entity import failed: " + res.error());
            report.errors++;
            continue;
        }
        report.imported++;
        recordLog(recNum, "info", "imported " + pe.type + " " + pe.extId);
    }

    // Links second.
    for (const auto& pl : links) {
        ++recNum;
        auto srcType = entityTypeFromString(pl.srcType);
        auto tgtType = entityTypeFromString(pl.tgtType);
        if (!srcType || !tgtType) {
            recordLog(recNum, "error", "invalid link type in relation");
            report.errors++;
            continue;
        }
        std::string srcId = resolveEntityId(toString(*srcType), pl.srcId);
        if (srcId.empty()) {
            recordLog(recNum, "error", "source entity not found: " + pl.srcId);
            report.errors++;
            continue;
        }
        std::string tgtId = resolveEntityId(toString(*tgtType), pl.tgtId);
        if (tgtId.empty()) {
            recordLog(recNum, "error", "target entity not found: " + pl.tgtId);
            report.errors++;
            continue;
        }
        Link l;
        l.sourceType = *srcType;
        l.sourceId = srcId;
        l.targetType = *tgtType;
        l.targetId = tgtId;
        l.relation = pl.relation;
        auto res = svc.addLink(l);
        if (res.failed()) {
            recordLog(recNum, "error", "link import failed: " + res.error());
            report.errors++;
            continue;
        }
        report.imported++;
        recordLog(recNum, "info",
                  "imported link " + pl.srcId + " -" + pl.relation + "-> " + pl.tgtId);
    }

    if (report.errors > 0) report.status = "partial";

    auto persist = persistBatch("reqif", report, log);
    if (persist.failed()) return common::Result<ImportReport>::err(persist.error());

    report.log = std::move(log);
    return common::Result<ImportReport>::ok(std::move(report));
}

}  // namespace lodestar::tracelink
