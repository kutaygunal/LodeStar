#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// core/tracelink/BaselineService.cpp
// WP-4 versioning: audit read, baseline snapshots, field-level diff, per-entity
// history, entity reconstruction at a baseline, and change-impact analysis.

#include "core/tracelink/BaselineService.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <map>
#include <set>
#include <utility>

#include <sqlite3.h>

#include "core/common/Uuid.h"
#include "core/tracelink/GraphEngine.h"
#include "core/tracelink/TraceLinkService.h"

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
// Minimal SQL helpers (local; the DAO layer's helpers are file-internal).
// ---------------------------------------------------------------------------
void bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()),
                      SQLITE_TRANSIENT);
}

std::string colText(sqlite3_stmt* stmt, int col) {
    const unsigned char* t = sqlite3_column_text(stmt, col);
    return t ? reinterpret_cast<const char*>(t) : std::string();
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

// Runs a SELECT expecting N text columns; returns one row per result.
common::Result<std::vector<std::vector<std::string>>> query(sqlite3* db,
                                                            const std::string& sql,
                                                            const std::vector<std::string>& params,
                                                            int ncols) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::vector<std::vector<std::string>>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db)));
    }
    for (size_t i = 0; i < params.size(); ++i) {
        bindText(stmt, static_cast<int>(i + 1), params[i]);
    }
    std::vector<std::vector<std::string>> out;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::vector<std::string> row;
        for (int c = 0; c < ncols; ++c) row.push_back(colText(stmt, c));
        out.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<std::vector<std::string>>>::ok(std::move(out));
}

// ---------------------------------------------------------------------------
// Tiny flat-JSON writer/parser (all values are JSON strings).
// ---------------------------------------------------------------------------
std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string jsonObject(const std::vector<std::pair<std::string, std::string>>& fields) {
    std::string s = "{";
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i) s += ",";
        s += "\"" + jsonEscape(fields[i].first) + "\":\"" + jsonEscape(fields[i].second) +
             "\"";
    }
    s += "}";
    return s;
}

std::map<std::string, std::string> parseFlatJson(const std::string& in) {
    std::map<std::string, std::string> out;
    size_t i = 0;
    while (i < in.size() && in[i] != '{') ++i;
    if (i < in.size()) ++i;  // consume '{'

    auto skipWs = [&]() {
        while (i < in.size() &&
               (in[i] == ' ' || in[i] == '\t' || in[i] == '\n' || in[i] == '\r'))
            ++i;
    };
    auto readString = [&](std::string& val) -> bool {
        if (i >= in.size() || in[i] != '"') return false;
        ++i;
        std::string v;
        while (i < in.size()) {
            char c = in[i];
            if (c == '"') {
                ++i;
                val = v;
                return true;
            }
            if (c == '\\') {
                ++i;
                if (i >= in.size()) return false;
                char e = in[i];
                if (e == '"') v += '"';
                else if (e == '\\') v += '\\';
                else if (e == 'n') v += '\n';
                else if (e == 'r') v += '\r';
                else if (e == 't') v += '\t';
                else if (e == 'u') {
                    if (i + 4 < in.size()) {
                        char hex[5] = {in[i + 1], in[i + 2], in[i + 3], in[i + 4], 0};
                        v += static_cast<char>(std::strtol(hex, nullptr, 16));
                        i += 4;
                    }
                } else {
                    v += e;
                }
                ++i;
            } else {
                v += c;
                ++i;
            }
        }
        return false;
    };

    while (i < in.size() && in[i] != '}') {
        skipWs();
        std::string key;
        if (!readString(key)) break;
        skipWs();
        if (i < in.size() && in[i] == ':') ++i;
        skipWs();
        std::string value;
        if (i < in.size() && in[i] == '"') {
            readString(value);
        } else {
            while (i < in.size() && in[i] != ',' && in[i] != '}') {
                value += in[i];
                ++i;
            }
        }
        out[key] = value;
        skipWs();
        if (i < in.size() && in[i] == ',') ++i;
    }
    return out;
}

int toInt(const std::string& s, int fallback = 0) {
    if (s.empty()) return fallback;
    try {
        return std::stoi(s);
    } catch (...) {
        return fallback;
    }
}

// ---------------------------------------------------------------------------
// Entity <-> JSON snapshot.
// ---------------------------------------------------------------------------
std::string serializeEntity(const Entity& e) {
    std::vector<std::pair<std::string, std::string>> f;
    f.push_back({"id", e.id});
    f.push_back({"externalId", e.externalId});
    f.push_back({"type", toString(e.type)});
    f.push_back({"name", e.name});
    f.push_back({"text", e.text});
    f.push_back({"status", e.status});
    f.push_back({"typeAttr", e.typeAttr});
    f.push_back({"priority", e.priority});
    f.push_back({"source", e.source});
    f.push_back({"owner", e.owner});
    f.push_back({"rationale", e.rationale});
    f.push_back({"verificationMethod", e.verificationMethod});
    f.push_back({"safetyLevel", e.safetyLevel});
    f.push_back({"direction", e.direction});
    f.push_back({"sourceEntity", e.sourceEntity});
    f.push_back({"targetEntity", e.targetEntity});
    f.push_back({"dataItems", e.dataItems});
    f.push_back({"protocol", e.protocol});
    f.push_back({"resultStatus", e.resultStatus});
    f.push_back({"severity", e.severity});
    f.push_back({"likelihood", e.likelihood});
    f.push_back({"date", e.date});
    f.push_back({"parentId", e.parentId});
    f.push_back({"sortOrder", std::to_string(e.sortOrder)});
    f.push_back({"tags", e.tags});
    f.push_back({"version", std::to_string(e.version)});
    f.push_back({"createdBy", e.createdBy});
    f.push_back({"createdAt", e.createdAt});
    f.push_back({"updatedBy", e.updatedBy});
    f.push_back({"updatedAt", e.updatedAt});
    return jsonObject(f);
}

Entity deserializeEntity(const std::string& snapshot, EntityType type) {
    auto m = parseFlatJson(snapshot);
    Entity e;
    e.type = type;
    auto get = [&](const std::string& k) -> std::string {
        auto it = m.find(k);
        return it == m.end() ? std::string() : it->second;
    };
    e.id = get("id");
    e.externalId = get("externalId");
    e.name = get("name");
    e.text = get("text");
    e.status = get("status");
    e.typeAttr = get("typeAttr");
    e.priority = get("priority");
    e.source = get("source");
    e.owner = get("owner");
    e.rationale = get("rationale");
    e.verificationMethod = get("verificationMethod");
    e.safetyLevel = get("safetyLevel");
    e.direction = get("direction");
    e.sourceEntity = get("sourceEntity");
    e.targetEntity = get("targetEntity");
    e.dataItems = get("dataItems");
    e.protocol = get("protocol");
    e.resultStatus = get("resultStatus");
    e.severity = get("severity");
    e.likelihood = get("likelihood");
    e.date = get("date");
    e.parentId = get("parentId");
    e.sortOrder = toInt(get("sortOrder"));
    e.tags = get("tags");
    e.version = toInt(get("version"), 1);
    e.createdBy = get("createdBy");
    e.createdAt = get("createdAt");
    e.updatedBy = get("updatedBy");
    e.updatedAt = get("updatedAt");
    return e;
}

std::string serializeLink(const Link& l) {
    std::vector<std::pair<std::string, std::string>> f;
    f.push_back({"id", l.id});
    f.push_back({"sourceType", toString(l.sourceType)});
    f.push_back({"sourceId", l.sourceId});
    f.push_back({"targetType", toString(l.targetType)});
    f.push_back({"targetId", l.targetId});
    f.push_back({"relation", l.relation});
    f.push_back({"rationale", l.rationale});
    f.push_back({"status", l.status});
    f.push_back({"createdBy", l.createdBy});
    f.push_back({"createdAt", l.createdAt});
    f.push_back({"updatedAt", l.updatedAt});
    f.push_back({"version", std::to_string(l.version)});
    f.push_back({"supersededBy", l.supersededBy});
    f.push_back({"validFrom", l.validFrom});
    f.push_back({"validTo", l.validTo});
    return jsonObject(f);
}

Link deserializeLink(const std::string& snapshot) {
    auto m = parseFlatJson(snapshot);
    Link l;
    auto get = [&](const std::string& k) -> std::string {
        auto it = m.find(k);
        return it == m.end() ? std::string() : it->second;
    };
    l.id = get("id");
    l.sourceType = entityTypeFromString(get("sourceType")).value_or(EntityType::Requirement);
    l.sourceId = get("sourceId");
    l.targetType = entityTypeFromString(get("targetType")).value_or(EntityType::Requirement);
    l.targetId = get("targetId");
    l.relation = get("relation");
    l.rationale = get("rationale");
    l.status = get("status");
    l.createdBy = get("createdBy");
    l.createdAt = get("createdAt");
    l.updatedAt = get("updatedAt");
    l.version = toInt(get("version"), 1);
    l.supersededBy = get("supersededBy");
    l.validFrom = get("validFrom");
    l.validTo = get("validTo");
    return l;
}

// ---------------------------------------------------------------------------
// Field-level diff between two entities (content fields only; identity and
// audit metadata are excluded so the diff shows exactly the real change).
// ---------------------------------------------------------------------------
void pushChangeIfDifferent(std::vector<FieldChange>& out, const std::string& field,
                           const std::string& a, const std::string& b) {
    if (a != b) {
        FieldChange fc;
        fc.field = field;
        fc.oldValue = a;
        fc.newValue = b;
        out.push_back(fc);
    }
}

std::vector<FieldChange> diffEntityFields(const Entity& a, const Entity& b) {
    std::vector<FieldChange> out;
    pushChangeIfDifferent(out, "externalId", a.externalId, b.externalId);
    pushChangeIfDifferent(out, "name", a.name, b.name);
    pushChangeIfDifferent(out, "text", a.text, b.text);
    pushChangeIfDifferent(out, "status", a.status, b.status);
    pushChangeIfDifferent(out, "typeAttr", a.typeAttr, b.typeAttr);
    pushChangeIfDifferent(out, "priority", a.priority, b.priority);
    pushChangeIfDifferent(out, "source", a.source, b.source);
    pushChangeIfDifferent(out, "owner", a.owner, b.owner);
    pushChangeIfDifferent(out, "rationale", a.rationale, b.rationale);
    pushChangeIfDifferent(out, "verificationMethod", a.verificationMethod, b.verificationMethod);
    pushChangeIfDifferent(out, "safetyLevel", a.safetyLevel, b.safetyLevel);
    pushChangeIfDifferent(out, "direction", a.direction, b.direction);
    pushChangeIfDifferent(out, "sourceEntity", a.sourceEntity, b.sourceEntity);
    pushChangeIfDifferent(out, "targetEntity", a.targetEntity, b.targetEntity);
    pushChangeIfDifferent(out, "dataItems", a.dataItems, b.dataItems);
    pushChangeIfDifferent(out, "protocol", a.protocol, b.protocol);
    pushChangeIfDifferent(out, "resultStatus", a.resultStatus, b.resultStatus);
    pushChangeIfDifferent(out, "severity", a.severity, b.severity);
    pushChangeIfDifferent(out, "likelihood", a.likelihood, b.likelihood);
    pushChangeIfDifferent(out, "date", a.date, b.date);
    pushChangeIfDifferent(out, "parentId", a.parentId, b.parentId);
    pushChangeIfDifferent(out, "tags", a.tags, b.tags);
    return out;
}

// ---------------------------------------------------------------------------
// Audit row readers.
// ---------------------------------------------------------------------------
AuditEntry entryFromRow(const std::vector<std::string>& r) {
    AuditEntry e;
    e.id = r[0];
    e.entityType = r[1];
    e.entityId = r[2];
    e.action = r[3];
    e.field = r[4];
    e.oldValue = r[5];
    e.newValue = r[6];
    e.actor = r[7];
    e.timestamp = r[8];
    e.changeRequestId = r[9];
    return e;
}

// ---------------------------------------------------------------------------
// Baseline restore helpers.
// ---------------------------------------------------------------------------
// Writes the exact snapshot state of an entity back into its table using
// INSERT OR REPLACE (restores existing rows and revives soft-deleted ones).
// The snapshot carries the full field set + version, so the row is reverted
// to the point-in-time state captured in the baseline.
common::Result<void> restoreEntityRow(sqlite3* db, EntityType type, const Entity& e) {
    std::vector<std::string> cols, vals;
    auto add = [&](const std::string& c, const std::string& v) {
        cols.push_back(c);
        vals.push_back(v);
    };
    add("id", e.id);
    add("external_id", e.externalId);
    add("name", e.name);
    add("description", e.text);
    add("status", e.status);
    add("version", std::to_string(e.version));
    add("created_by", e.createdBy);
    add("created_at", e.createdAt);
    add("updated_by", e.updatedBy);
    add("updated_at", e.updatedAt);

    std::string table;
    switch (type) {
        case EntityType::Requirement:
            table = "requirements";
            add("type", e.typeAttr.empty() ? "functional" : e.typeAttr);
            add("priority", e.priority);
            add("source", e.source);
            add("owner", e.owner);
            add("rationale", e.rationale);
            add("verification_method", e.verificationMethod);
            add("safety_level", e.safetyLevel);
            add("parent_id", e.parentId);
            add("sort_order", std::to_string(e.sortOrder));
            add("tags", e.tags);
            break;
        case EntityType::Design:
            table = "design_items";
            add("type", e.typeAttr.empty() ? "component" : e.typeAttr);
            add("owner", e.owner);
            add("parent_id", e.parentId);
            add("tags", e.tags);
            break;
        case EntityType::Interface:
            table = "interfaces";
            add("direction", e.direction);
            add("source_entity", e.sourceEntity);
            add("target_entity", e.targetEntity);
            add("data_items", e.dataItems);
            add("protocol", e.protocol);
            add("parent_id", e.parentId);
            add("sort_order", std::to_string(e.sortOrder));
            add("tags", e.tags);
            break;
        case EntityType::TestCase:
            table = "test_cases";
            add("verification_method", e.verificationMethod);
            add("result_status", e.resultStatus);
            add("priority", e.priority);
            add("parent_id", e.parentId);
            add("sort_order", std::to_string(e.sortOrder));
            add("tags", e.tags);
            break;
        case EntityType::Hazard:
            table = "hazards";
            add("severity", e.severity);
            add("likelihood", e.likelihood);
            add("owner", e.owner);
            add("parent_id", e.parentId);
            add("sort_order", std::to_string(e.sortOrder));
            add("tags", e.tags);
            break;
        case EntityType::Decision:
            table = "decisions";
            add("rationale", e.rationale);
            add("owner", e.owner);
            add("date", e.date);
            add("parent_id", e.parentId);
            add("sort_order", std::to_string(e.sortOrder));
            add("tags", e.tags);
            break;
        case EntityType::Assumption:
            table = "assumptions";
            add("owner", e.owner);
            add("parent_id", e.parentId);
            add("sort_order", std::to_string(e.sortOrder));
            add("tags", e.tags);
            break;
    }

    std::string sql = "INSERT OR REPLACE INTO " + table + " (";
    for (size_t i = 0; i < cols.size(); ++i) {
        if (i) sql += ",";
        sql += cols[i];
    }
    sql += ") VALUES (";
    for (size_t i = 0; i < cols.size(); ++i) {
        if (i) sql += ",";
        sql += "?";
    }
    sql += ");";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<void>::err("prepare failed: " +
                                         std::string(sqlite3_errmsg(db)));
    }
    for (size_t i = 0; i < vals.size(); ++i) {
        bindText(stmt, static_cast<int>(i + 1), vals[i]);
    }
    int rc = sqlite3_step(stmt);
    std::string msg = sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err("restore entity failed: " + msg);
    }
    return common::Result<void>::ok();
}

// Writes the exact snapshot state of a link back into trace_links using
// INSERT OR REPLACE (restores the link to Active with its snapshot fields).
common::Result<void> restoreLinkRow(sqlite3* db, const Link& l) {
    std::vector<std::string> cols = {
        "id", "source_type", "source_id", "target_type", "target_id",
        "relation", "rationale", "status", "created_by", "created_at",
        "updated_at", "version", "superseded_by", "valid_from", "valid_to"};
    std::vector<std::string> vals = {
        l.id, toString(l.sourceType), l.sourceId, toString(l.targetType),
        l.targetId, l.relation, l.rationale, l.status, l.createdBy, l.createdAt,
        l.updatedAt, std::to_string(l.version), l.supersededBy, l.validFrom,
        l.validTo};
    std::string sql = "INSERT OR REPLACE INTO trace_links (";
    for (size_t i = 0; i < cols.size(); ++i) {
        if (i) sql += ",";
        sql += cols[i];
    }
    sql += ") VALUES (";
    for (size_t i = 0; i < cols.size(); ++i) {
        if (i) sql += ",";
        sql += "?";
    }
    sql += ");";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<void>::err("prepare failed: " +
                                         std::string(sqlite3_errmsg(db)));
    }
    for (size_t i = 0; i < vals.size(); ++i) {
        bindText(stmt, static_cast<int>(i + 1), vals[i]);
    }
    int rc = sqlite3_step(stmt);
    std::string msg = sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err("restore link failed: " + msg);
    }
    return common::Result<void>::ok();
}

}  // namespace

// ---------------------------------------------------------------------------
// BaselineService.
// ---------------------------------------------------------------------------
BaselineService::BaselineService(persistence::Database& db) : db_(db) {}

common::Result<std::vector<AuditEntry>> BaselineService::history(EntityType type,
                                                                 const std::string& id) {
    auto rows = query(db_.handle(),
                      "SELECT id, entity_type, entity_id, action, field, old_value, "
                      "new_value, actor, timestamp, change_request_id FROM audit_log "
                      "WHERE entity_type=? AND entity_id=? ORDER BY rowid ASC;",
                      {toString(type), id}, 10);
    if (rows.failed()) return common::Result<std::vector<AuditEntry>>::err(rows.error());
    std::vector<AuditEntry> out;
    for (const auto& r : rows.value()) out.push_back(entryFromRow(r));
    return common::Result<std::vector<AuditEntry>>::ok(std::move(out));
}

common::Result<std::vector<AuditEntry>> BaselineService::allHistory() {
    auto rows = query(db_.handle(),
                      "SELECT id, entity_type, entity_id, action, field, old_value, "
                      "new_value, actor, timestamp, change_request_id FROM audit_log "
                      "ORDER BY rowid ASC;",
                      {}, 10);
    if (rows.failed()) return common::Result<std::vector<AuditEntry>>::err(rows.error());
    std::vector<AuditEntry> out;
    for (const auto& r : rows.value()) out.push_back(entryFromRow(r));
    return common::Result<std::vector<AuditEntry>>::ok(std::move(out));
}

common::Result<Baseline> BaselineService::createBaseline(const std::string& name,
                                                         const std::string& description) {
    TraceLinkService svc(db_);
    Baseline bl;
    bl.id = newUuid();
    bl.name = name;
    bl.description = description;
    bl.createdAt = now();

    auto begin = db_.execute("BEGIN IMMEDIATE;");
    if (begin.failed()) return common::Result<Baseline>::err("BEGIN failed");
    auto insertBl = exec(db_.handle(),
                         "INSERT INTO baselines (id, name, description, created_by, "
                         "created_at) VALUES (?,?,?,?,?);",
                         {bl.id, bl.name, bl.description, "", bl.createdAt});
    if (insertBl.failed()) {
        db_.execute("ROLLBACK;");
        return common::Result<Baseline>::err(insertBl.error());
    }

    // Snapshot every active entity.
    const std::vector<EntityType> allTypes = {
        EntityType::Requirement, EntityType::Design,     EntityType::Interface,
        EntityType::TestCase,    EntityType::Hazard,     EntityType::Decision,
        EntityType::Assumption};
    for (auto t : allTypes) {
        auto list = svc.listEntities(t, EntityFilter{});
        if (list.failed()) {
            db_.execute("ROLLBACK;");
            return common::Result<Baseline>::err(list.error());
        }
        for (const auto& e : list.value()) {
            if (e.status == "Obsolete") continue;  // active only
            auto ins = exec(db_.handle(),
                            "INSERT OR REPLACE INTO baseline_entities (baseline_id, "
                            "entity_type, entity_id, version, snapshot) VALUES (?,?,?,?,?);",
                            {bl.id, toString(t), e.id, std::to_string(e.version),
                             serializeEntity(e)});
            if (ins.failed()) {
                db_.execute("ROLLBACK;");
                return common::Result<Baseline>::err(ins.error());
            }
        }
    }

    // Snapshot every active link.
    auto links = svc.allLinks();
    if (links.failed()) {
        db_.execute("ROLLBACK;");
        return common::Result<Baseline>::err(links.error());
    }
    for (const auto& l : links.value()) {
        if (l.status == "Superseded") continue;  // active only
        auto ins = exec(db_.handle(),
                        "INSERT OR REPLACE INTO baseline_links (baseline_id, link_id, "
                        "snapshot) VALUES (?,?,?);",
                        {bl.id, l.id, serializeLink(l)});
        if (ins.failed()) {
            db_.execute("ROLLBACK;");
            return common::Result<Baseline>::err(ins.error());
        }
    }

    auto commit = db_.execute("COMMIT;");
    if (commit.failed()) {
        db_.execute("ROLLBACK;");
        return common::Result<Baseline>::err("COMMIT failed");
    }
    return common::Result<Baseline>::ok(std::move(bl));
}

common::Result<std::vector<Baseline>> BaselineService::listBaselines() {
    auto rows = query(db_.handle(),
                      "SELECT id, name, description, created_at FROM baselines "
                      "ORDER BY created_at, rowid ASC;",
                      {}, 4);
    if (rows.failed()) return common::Result<std::vector<Baseline>>::err(rows.error());
    std::vector<Baseline> out;
    for (const auto& r : rows.value()) {
        Baseline b;
        b.id = r[0];
        b.name = r[1];
        b.description = r[2];
        b.createdAt = r[3];
        out.push_back(std::move(b));
    }
    return common::Result<std::vector<Baseline>>::ok(std::move(out));
}

common::Result<std::optional<Baseline>> BaselineService::getBaseline(const std::string& id) {
    auto rows = query(db_.handle(),
                      "SELECT id, name, description, created_at FROM baselines WHERE id=?;",
                      {id}, 4);
    if (rows.failed()) return common::Result<std::optional<Baseline>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<Baseline>>::ok(std::nullopt);
    const auto& r = rows.value().front();
    Baseline b;
    b.id = r[0];
    b.name = r[1];
    b.description = r[2];
    b.createdAt = r[3];
    return common::Result<std::optional<Baseline>>::ok(std::move(b));
}

common::Result<DiffResult> BaselineService::diffBaseline(const std::string& aId,
                                                         const std::string& bId) {
    // entity_type, entity_id, version, snapshot
    auto aEnt = query(db_.handle(),
                      "SELECT entity_type, entity_id, version, snapshot FROM "
                      "baseline_entities WHERE baseline_id=?;",
                      {aId}, 4);
    if (aEnt.failed()) return common::Result<DiffResult>::err(aEnt.error());
    auto bEnt = query(db_.handle(),
                      "SELECT entity_type, entity_id, version, snapshot FROM "
                      "baseline_entities WHERE baseline_id=?;",
                      {bId}, 4);
    if (bEnt.failed()) return common::Result<DiffResult>::err(bEnt.error());

    // link_id, snapshot
    auto aLnk = query(db_.handle(),
                      "SELECT link_id, snapshot FROM baseline_links WHERE baseline_id=?;",
                      {aId}, 2);
    if (aLnk.failed()) return common::Result<DiffResult>::err(aLnk.error());
    auto bLnk = query(db_.handle(),
                      "SELECT link_id, snapshot FROM baseline_links WHERE baseline_id=?;",
                      {bId}, 2);
    if (bLnk.failed()) return common::Result<DiffResult>::err(bLnk.error());

    DiffResult result;

    // Entities.
    auto key = [](const std::string& t, const std::string& id) { return t + ":" + id; };
    std::map<std::string, std::vector<std::string>> aMap, bMap;
    for (const auto& r : aEnt.value()) aMap[key(r[0], r[1])] = r;
    for (const auto& r : bEnt.value()) bMap[key(r[0], r[1])] = r;

    std::vector<std::string> keys;
    for (const auto& [k, v] : aMap) keys.push_back(k);
    for (const auto& [k, v] : bMap) {
        if (aMap.find(k) == aMap.end()) keys.push_back(k);
    }
    std::sort(keys.begin(), keys.end());

    for (const auto& k : keys) {
        auto typeOpt = entityTypeFromString(aMap[k].empty() ? bMap[k][0] : aMap[k][0]);
        EntityType type = typeOpt.value_or(EntityType::Requirement);
        auto aIt = aMap.find(k);
        auto bIt = bMap.find(k);
        DiffEntry de;
        if (aIt == aMap.end()) {
            de.kind = DiffKind::Added;
            de.entityType = type;
            de.entityId = bIt->second[1];
        } else if (bIt == bMap.end()) {
            de.kind = DiffKind::Removed;
            de.entityType = type;
            de.entityId = aIt->second[1];
        } else {
            Entity ea = deserializeEntity(aIt->second[3], type);
            Entity eb = deserializeEntity(bIt->second[3], type);
            auto changes = diffEntityFields(ea, eb);
            if (!changes.empty()) {
                de.kind = DiffKind::Modified;
                de.entityType = type;
                de.entityId = ea.id.empty() ? eb.id : ea.id;
                de.fieldChanges = std::move(changes);
            } else {
                continue;  // identical, not a difference
            }
        }
        // external id is not stored on a DiffEntry column; resolve from entity id
        // via the snapshot for readability (best-effort, unused by tests).
        const std::vector<std::string>* ref = nullptr;
        if (aIt != aMap.end()) ref = &aIt->second;
        else if (bIt != bMap.end()) ref = &bIt->second;
        if (ref) {
            Entity tmp = deserializeEntity((*ref)[3], de.entityType);
            de.entityExternalId = tmp.externalId;
        }
        result.entities.push_back(std::move(de));
    }

    // Links (added/removed/modified; external id empty for links).
    std::map<std::string, std::string> aLinkMap, bLinkMap;
    for (const auto& r : aLnk.value()) aLinkMap[r[0]] = r[1];
    for (const auto& r : bLnk.value()) bLinkMap[r[0]] = r[1];

    std::vector<std::string> linkKeys;
    for (const auto& [k, v] : aLinkMap) linkKeys.push_back(k);
    for (const auto& [k, v] : bLinkMap) {
        if (aLinkMap.find(k) == aLinkMap.end()) linkKeys.push_back(k);
    }
    std::sort(linkKeys.begin(), linkKeys.end());

    for (const auto& k : linkKeys) {
        auto aIt = aLinkMap.find(k);
        auto bIt = bLinkMap.find(k);
        DiffEntry de;
        de.entityType = EntityType::Requirement;  // links carry no EntityType
        de.entityId = k;
        if (aIt == aLinkMap.end()) {
            de.kind = DiffKind::Added;
        } else if (bIt == bLinkMap.end()) {
            de.kind = DiffKind::Removed;
        } else {
            Link la = deserializeLink(aIt->second);
            Link lb = deserializeLink(bIt->second);
            std::vector<FieldChange> fc;
            pushChangeIfDifferent(fc, "sourceType", toString(la.sourceType),
                                  toString(lb.sourceType));
            pushChangeIfDifferent(fc, "sourceId", la.sourceId, lb.sourceId);
            pushChangeIfDifferent(fc, "targetType", toString(la.targetType),
                                  toString(lb.targetType));
            pushChangeIfDifferent(fc, "targetId", la.targetId, lb.targetId);
            pushChangeIfDifferent(fc, "relation", la.relation, lb.relation);
            pushChangeIfDifferent(fc, "status", la.status, lb.status);
            pushChangeIfDifferent(fc, "rationale", la.rationale, lb.rationale);
            if (fc.empty()) continue;
            de.kind = DiffKind::Modified;
            de.fieldChanges = std::move(fc);
        }
        result.links.push_back(std::move(de));
    }

    return common::Result<DiffResult>::ok(std::move(result));
}

common::Result<std::optional<Entity>> BaselineService::entityAtBaseline(
    EntityType type, const std::string& id, const std::string& baselineId) {
    auto rows = query(db_.handle(),
                      "SELECT snapshot FROM baseline_entities WHERE baseline_id=? AND "
                      "entity_type=? AND entity_id=?;",
                      {baselineId, toString(type), id}, 1);
    if (rows.failed()) return common::Result<std::optional<Entity>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<Entity>>::ok(std::nullopt);
    Entity e = deserializeEntity(rows.value().front()[0], type);
    return common::Result<std::optional<Entity>>::ok(std::move(e));
}

common::Result<ImpactResult> BaselineService::changeImpact(
    EntityType type, const std::string& id, const std::string& changeRequestId) {
    ImpactResult result;

    // Audit entries tagged to the change request.
    auto rows = query(db_.handle(),
                      "SELECT id, entity_type, entity_id, action, field, old_value, "
                      "new_value, actor, timestamp, change_request_id FROM audit_log "
                      "WHERE change_request_id=? ORDER BY rowid ASC;",
                      {changeRequestId}, 10);
    if (rows.failed()) return common::Result<ImpactResult>::err(rows.error());
    for (const auto& r : rows.value()) result.changes.push_back(entryFromRow(r));

    // Downstream affected entities (transitive closure excluding self).
    GraphEngine ge(db_);
    auto down = ge.downstreamClosure(type, id);
    if (down.failed()) return common::Result<ImpactResult>::err(down.error());
    result.downstreamAffected = std::move(down.value());

    return common::Result<ImpactResult>::ok(std::move(result));
}

common::Result<int> BaselineService::restoreBaseline(const std::string& baselineId) {
    // The baseline must exist.
    auto bl = getBaseline(baselineId);
    if (bl.failed()) return common::Result<int>::err(bl.error());
    if (!bl.value()) {
        return common::Result<int>::err(common::ErrorCode::NotFound,
                                        "baseline not found: " + baselineId);
    }

    // Entity snapshots for this baseline.
    auto entRows = query(db_.handle(),
                         "SELECT entity_type, entity_id, snapshot FROM "
                         "baseline_entities WHERE baseline_id=?;",
                         {baselineId}, 3);
    if (entRows.failed()) return common::Result<int>::err(entRows.error());

    // Link snapshots for this baseline.
    auto linkRows = query(db_.handle(),
                          "SELECT link_id, snapshot FROM baseline_links "
                          "WHERE baseline_id=?;",
                          {baselineId}, 2);
    if (linkRows.failed()) return common::Result<int>::err(linkRows.error());

    auto begin = db_.execute("BEGIN IMMEDIATE;");
    if (begin.failed()) return common::Result<int>::err("BEGIN failed");

    int restored = 0;
    for (const auto& r : entRows.value()) {
        auto typeOpt = entityTypeFromString(r[0]);
        if (!typeOpt) continue;
        Entity e = deserializeEntity(r[2], *typeOpt);
        auto res = restoreEntityRow(db_.handle(), *typeOpt, e);
        if (res.failed()) {
            db_.execute("ROLLBACK;");
            return common::Result<int>::err(res.error());
        }
        ++restored;
    }

    // Revert the link set: restore every snapshot link and mark any current
    // link that is not part of the baseline as Superseded.
    std::set<std::string> baselineLinkIds;
    for (const auto& r : linkRows.value()) {
        baselineLinkIds.insert(r[0]);
        Link l = deserializeLink(r[1]);
        auto res = restoreLinkRow(db_.handle(), l);
        if (res.failed()) {
            db_.execute("ROLLBACK;");
            return common::Result<int>::err(res.error());
        }
    }
    auto currentLinks = query(db_.handle(), "SELECT id FROM trace_links;", {}, 1);
    if (currentLinks.failed()) {
        db_.execute("ROLLBACK;");
        return common::Result<int>::err(currentLinks.error());
    }
    for (const auto& r : currentLinks.value()) {
        if (baselineLinkIds.find(r[0]) != baselineLinkIds.end()) continue;
        auto res = exec(db_.handle(),
                        "UPDATE trace_links SET status='Superseded' WHERE id=?;",
                        {r[0]});
        if (res.failed()) {
            db_.execute("ROLLBACK;");
            return common::Result<int>::err(res.error());
        }
    }

    auto commit = db_.execute("COMMIT;");
    if (commit.failed()) {
        db_.execute("ROLLBACK;");
        return common::Result<int>::err("COMMIT failed");
    }
    return common::Result<int>::ok(restored);
}

}  // namespace lodestar::tracelink
