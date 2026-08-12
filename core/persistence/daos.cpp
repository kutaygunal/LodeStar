// core/persistence/daos.cpp
// DAO layer wrapping SQLite for every TraceLink entity type and the link table.

#include "core/persistence/daos.h"

#include <functional>
#include <map>
#include <sstream>

#include <sqlite3.h>

namespace lodestar::persistence {

namespace {

using Row = std::map<std::string, std::string>;

// ---------------------------------------------------------------------------
// SQLite helpers.
// ---------------------------------------------------------------------------
void bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()),
                      SQLITE_TRANSIENT);
}

std::string columnText(sqlite3_stmt* stmt, int col) {
    const unsigned char* text = sqlite3_column_text(stmt, col);
    return text ? reinterpret_cast<const char*>(text) : std::string();
}

std::string join(const std::vector<std::string>& v, const char* sep = ",") {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) s += sep;
        s += v[i];
    }
    return s;
}

std::string placeholders(size_t n) {
    std::string s;
    for (size_t i = 0; i < n; ++i) {
        if (i) s += ",";
        s += "?";
    }
    return s;
}

int toInt(const std::string& s, int fallback = 0) {
    if (s.empty()) return fallback;
    try {
        return std::stoi(s);
    } catch (...) {
        return fallback;
    }
}

common::Result<void> execBinds(sqlite3* db, const std::string& sql,
                               const std::vector<std::string>& params) {
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

// Generic SELECT returning rows keyed by column name, in `columns` order.
common::Result<std::vector<Row>> selectRows(sqlite3* db, const std::string& table,
                                            const std::vector<std::string>& columns,
                                            const std::string& where,
                                            const std::vector<std::string>& params,
                                            const std::string& suffix = "") {
    std::string sql = "SELECT " + join(columns) + " FROM " + table;
    if (!where.empty()) sql += " WHERE " + where;
    sql += suffix;
    sql += ";";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::vector<Row>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db)));
    }
    for (size_t i = 0; i < params.size(); ++i) {
        bindText(stmt, static_cast<int>(i + 1), params[i]);
    }
    std::vector<Row> out;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Row row;
        for (size_t c = 0; c < columns.size(); ++c) {
            row[columns[c]] = columnText(stmt, static_cast<int>(c));
        }
        out.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<Row>>::ok(std::move(out));
}

common::Result<void> insertInto(sqlite3* db, const std::string& table,
                                const std::vector<std::string>& columns,
                                const std::vector<std::string>& values) {
    std::string sql = "INSERT INTO " + table + " (" + join(columns) + ") VALUES (" +
                      placeholders(columns.size()) + ");";
    return execBinds(db, sql, values);
}

// Updates the given columns (the id column, always index 0 in these column
// lists, is excluded from SET and used as the WHERE key).
common::Result<void> updateById(sqlite3* db, const std::string& table,
                                const std::vector<std::string>& fullCols,
                                const std::vector<std::string>& fullValues,
                                const std::string& id) {
    std::vector<std::string> columns, values;
    for (size_t i = 0; i < fullCols.size(); ++i) {
        if (fullCols[i] == "id") continue;  // keep positional alignment
        columns.push_back(fullCols[i]);
        values.push_back(fullValues[i]);
    }
    std::string sql = "UPDATE " + table + " SET ";
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i) sql += ",";
        sql += columns[i] + "=?";
    }
    sql += " WHERE id=?;";
    std::vector<std::string> params = values;
    params.push_back(id);
    return execBinds(db, sql, params);
}

common::Result<void> softDeleteRow(sqlite3* db, const std::string& table,
                                   const std::string& id) {
    std::string sql = "UPDATE " + table + " SET status='Obsolete' WHERE id=?;";
    return execBinds(db, sql, {id});
}

std::string buildWhere(const EntityFilter& f, std::vector<std::string>& params) {
    std::vector<std::string> conds;
    if (!f.status.empty()) {
        conds.push_back("status=?");
        params.push_back(f.status);
    }
    if (!f.tags.empty()) {
        conds.push_back("tags LIKE ?");
        params.push_back("%" + f.tags + "%");
    }
    if (!f.text.empty()) {
        conds.push_back("(name LIKE ? OR description LIKE ?)");
        params.push_back("%" + f.text + "%");
        params.push_back("%" + f.text + "%");
    }
    if (conds.empty()) return "";
    return join(conds, " AND ");
}

std::string filterSuffix(const EntityFilter& f) {
    std::string suffix = " ORDER BY name";
    if (f.limit > 0) {
        suffix += " LIMIT " + std::to_string(f.limit);
        if (f.offset > 0) suffix += " OFFSET " + std::to_string(f.offset);
    }
    return suffix;
}

template <typename T>
common::Result<std::vector<T>> queryFiltered(sqlite3* db, const std::string& table,
                                             const std::vector<std::string>& cols,
                                             const EntityFilter& f,
                                             const std::function<T(const Row&)>& fromRow) {
    std::vector<std::string> params;
    std::string where = buildWhere(f, params);
    auto rows = selectRows(db, table, cols, where, params, filterSuffix(f));
    if (rows.failed()) {
        return common::Result<std::vector<T>>::err(rows.error());
    }
    std::vector<T> out;
    for (const auto& row : rows.value()) out.push_back(fromRow(row));
    return common::Result<std::vector<T>>::ok(std::move(out));
}

// ---------------------------------------------------------------------------
// Column lists (canonical select/insert order) and row readers/writers.
// ---------------------------------------------------------------------------
const std::vector<std::string> kReqCols = {
    "id", "external_id", "name", "description", "type", "status", "priority",
    "source", "owner", "rationale", "verification_method", "safety_level",
    "parent_id", "sort_order", "tags", "version", "created_by", "created_at",
    "updated_by", "updated_at"};

const std::vector<std::string> kDesignCols = {
    "id", "external_id", "name", "description", "type", "status", "owner",
    "parent_id", "tags", "version", "created_by", "created_at", "updated_by",
    "updated_at"};

const std::vector<std::string> kInterfaceCols = {
    "id", "external_id", "name", "description", "status", "direction",
    "source_entity", "target_entity", "data_items", "protocol", "tags",
    "version", "created_by", "created_at", "updated_by", "updated_at"};

const std::vector<std::string> kTestCaseCols = {
    "id", "external_id", "name", "description", "status", "verification_method",
    "result_status", "priority", "tags", "version", "created_by", "created_at",
    "updated_by", "updated_at"};

const std::vector<std::string> kHazardCols = {
    "id", "external_id", "name", "description", "status", "severity",
    "likelihood", "owner", "tags", "version", "created_by", "created_at",
    "updated_by", "updated_at"};

const std::vector<std::string> kDecisionCols = {
    "id", "external_id", "name", "description", "status", "rationale", "owner",
    "date", "tags", "version", "created_by", "created_at", "updated_by",
    "updated_at"};

const std::vector<std::string> kAssumptionCols = {
    "id", "external_id", "name", "description", "status", "owner", "tags",
    "version", "created_by", "created_at", "updated_by", "updated_at"};

const std::vector<std::string> kLinkCols = {
    "id", "source_type", "source_id", "target_type", "target_id", "relation",
    "rationale", "status", "created_by", "created_at", "updated_at", "version",
    "superseded_by", "valid_from", "valid_to"};

// (the id column is excluded from UPDATE via updateById).
Requirement fromRequirementRow(const Row& row) {
    Requirement r;
    r.id = row.at("id");
    r.externalId = row.at("external_id");
    r.name = row.at("name");
    r.description = row.at("description");
    r.type = row.at("type");
    r.status = row.at("status");
    r.priority = row.at("priority");
    r.source = row.at("source");
    r.owner = row.at("owner");
    r.rationale = row.at("rationale");
    r.verificationMethod = row.at("verification_method");
    r.safetyLevel = row.at("safety_level");
    r.parentId = row.at("parent_id");
    r.sortOrder = toInt(row.at("sort_order"));
    r.tags = row.at("tags");
    r.version = toInt(row.at("version"), 1);
    r.createdBy = row.at("created_by");
    r.createdAt = row.at("created_at");
    r.updatedBy = row.at("updated_by");
    r.updatedAt = row.at("updated_at");
    return r;
}

std::vector<std::string> requirementValues(const Requirement& r) {
    return {r.id,      r.externalId,       r.name,   r.description,
            r.type,    r.status,           r.priority, r.source,
            r.owner,   r.rationale,        r.verificationMethod, r.safetyLevel,
            r.parentId, std::to_string(r.sortOrder), r.tags,
            std::to_string(r.version),     r.createdBy, r.createdAt,
            r.updatedBy, r.updatedAt};
}

DesignItem fromDesignRow(const Row& row) {
    DesignItem d;
    d.id = row.at("id");
    d.externalId = row.at("external_id");
    d.name = row.at("name");
    d.description = row.at("description");
    d.type = row.at("type");
    d.status = row.at("status");
    d.owner = row.at("owner");
    d.parentId = row.at("parent_id");
    d.tags = row.at("tags");
    d.version = toInt(row.at("version"), 1);
    d.createdBy = row.at("created_by");
    d.createdAt = row.at("created_at");
    d.updatedBy = row.at("updated_by");
    d.updatedAt = row.at("updated_at");
    return d;
}

std::vector<std::string> designValues(const DesignItem& d) {
    return {d.id,       d.externalId, d.name, d.description, d.type, d.status,
            d.owner,    d.parentId,   d.tags, std::to_string(d.version),
            d.createdBy, d.createdAt, d.updatedBy, d.updatedAt};
}

InterfaceDef fromInterfaceRow(const Row& row) {
    InterfaceDef i;
    i.id = row.at("id");
    i.externalId = row.at("external_id");
    i.name = row.at("name");
    i.description = row.at("description");
    i.status = row.at("status");
    i.direction = row.at("direction");
    i.sourceEntity = row.at("source_entity");
    i.targetEntity = row.at("target_entity");
    i.dataItems = row.at("data_items");
    i.protocol = row.at("protocol");
    i.tags = row.at("tags");
    i.version = toInt(row.at("version"), 1);
    i.createdBy = row.at("created_by");
    i.createdAt = row.at("created_at");
    i.updatedBy = row.at("updated_by");
    i.updatedAt = row.at("updated_at");
    return i;
}

std::vector<std::string> interfaceValues(const InterfaceDef& i) {
    return {i.id,          i.externalId,  i.name,      i.description,
            i.status,      i.direction,   i.sourceEntity, i.targetEntity,
            i.dataItems,   i.protocol,    i.tags,      std::to_string(i.version),
            i.createdBy,   i.createdAt,   i.updatedBy, i.updatedAt};
}

TestCase fromTestCaseRow(const Row& row) {
    TestCase t;
    t.id = row.at("id");
    t.externalId = row.at("external_id");
    t.name = row.at("name");
    t.description = row.at("description");
    t.status = row.at("status");
    t.verificationMethod = row.at("verification_method");
    t.resultStatus = row.at("result_status");
    t.priority = row.at("priority");
    t.tags = row.at("tags");
    t.version = toInt(row.at("version"), 1);
    t.createdBy = row.at("created_by");
    t.createdAt = row.at("created_at");
    t.updatedBy = row.at("updated_by");
    t.updatedAt = row.at("updated_at");
    return t;
}

std::vector<std::string> testCaseValues(const TestCase& t) {
    return {t.id,       t.externalId, t.name, t.description, t.status,
            t.verificationMethod, t.resultStatus, t.priority, t.tags,
            std::to_string(t.version), t.createdBy, t.createdAt, t.updatedBy,
            t.updatedAt};
}

Hazard fromHazardRow(const Row& row) {
    Hazard h;
    h.id = row.at("id");
    h.externalId = row.at("external_id");
    h.name = row.at("name");
    h.description = row.at("description");
    h.status = row.at("status");
    h.severity = row.at("severity");
    h.likelihood = row.at("likelihood");
    h.owner = row.at("owner");
    h.tags = row.at("tags");
    h.version = toInt(row.at("version"), 1);
    h.createdBy = row.at("created_by");
    h.createdAt = row.at("created_at");
    h.updatedBy = row.at("updated_by");
    h.updatedAt = row.at("updated_at");
    return h;
}

std::vector<std::string> hazardValues(const Hazard& h) {
    return {h.id,       h.externalId, h.name, h.description, h.status,
            h.severity, h.likelihood, h.owner, h.tags, std::to_string(h.version),
            h.createdBy, h.createdAt, h.updatedBy, h.updatedAt};
}

Decision fromDecisionRow(const Row& row) {
    Decision d;
    d.id = row.at("id");
    d.externalId = row.at("external_id");
    d.name = row.at("name");
    d.description = row.at("description");
    d.status = row.at("status");
    d.rationale = row.at("rationale");
    d.owner = row.at("owner");
    d.date = row.at("date");
    d.tags = row.at("tags");
    d.version = toInt(row.at("version"), 1);
    d.createdBy = row.at("created_by");
    d.createdAt = row.at("created_at");
    d.updatedBy = row.at("updated_by");
    d.updatedAt = row.at("updated_at");
    return d;
}

std::vector<std::string> decisionValues(const Decision& d) {
    return {d.id,       d.externalId, d.name, d.description, d.status,
            d.rationale, d.owner,     d.date, d.tags, std::to_string(d.version),
            d.createdBy, d.createdAt, d.updatedBy, d.updatedAt};
}

Assumption fromAssumptionRow(const Row& row) {
    Assumption a;
    a.id = row.at("id");
    a.externalId = row.at("external_id");
    a.name = row.at("name");
    a.description = row.at("description");
    a.status = row.at("status");
    a.owner = row.at("owner");
    a.tags = row.at("tags");
    a.version = toInt(row.at("version"), 1);
    a.createdBy = row.at("created_by");
    a.createdAt = row.at("created_at");
    a.updatedBy = row.at("updated_by");
    a.updatedAt = row.at("updated_at");
    return a;
}

std::vector<std::string> assumptionValues(const Assumption& a) {
    return {a.id,       a.externalId, a.name, a.description, a.status, a.owner,
            a.tags,     std::to_string(a.version), a.createdBy, a.createdAt,
            a.updatedBy, a.updatedAt};
}

TraceLink fromLinkRow(const Row& row) {
    TraceLink l;
    l.id = row.at("id");
    l.sourceType = row.at("source_type");
    l.sourceId = row.at("source_id");
    l.targetType = row.at("target_type");
    l.targetId = row.at("target_id");
    l.relation = row.at("relation");
    l.rationale = row.at("rationale");
    l.status = row.at("status");
    l.createdBy = row.at("created_by");
    l.createdAt = row.at("created_at");
    l.updatedAt = row.at("updated_at");
    l.version = toInt(row.at("version"), 1);
    l.supersededBy = row.at("superseded_by");
    l.validFrom = row.at("valid_from");
    l.validTo = row.at("valid_to");
    return l;
}

std::vector<std::string> linkValues(const TraceLink& l) {
    return {l.id,          l.sourceType, l.sourceId, l.targetType, l.targetId,
            l.relation,    l.rationale,  l.status,   l.createdBy,  l.createdAt,
            l.updatedAt,   std::to_string(l.version), l.supersededBy,
            l.validFrom,   l.validTo};
}

}  // namespace

// ---------------------------------------------------------------------------
// RequirementDao
// ---------------------------------------------------------------------------
common::Result<void> RequirementDao::create(const Requirement& r) {
    return insertInto(db_.handle(), "requirements", kReqCols, requirementValues(r));
}

common::Result<std::optional<Requirement>> RequirementDao::findById(const std::string& id) {
    auto rows = selectRows(db_.handle(), "requirements", kReqCols, "id=?", {id});
    if (rows.failed()) return common::Result<std::optional<Requirement>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<Requirement>>::ok(std::nullopt);
    return common::Result<std::optional<Requirement>>::ok(fromRequirementRow(rows.value().front()));
}

common::Result<std::optional<Requirement>> RequirementDao::findByExternalId(const std::string& extId) {
    auto rows = selectRows(db_.handle(), "requirements", kReqCols, "external_id=?", {extId});
    if (rows.failed()) return common::Result<std::optional<Requirement>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<Requirement>>::ok(std::nullopt);
    return common::Result<std::optional<Requirement>>::ok(fromRequirementRow(rows.value().front()));
}

common::Result<std::vector<Requirement>> RequirementDao::findAll() {
    auto rows = selectRows(db_.handle(), "requirements", kReqCols, "status != 'Obsolete'", {}, " ORDER BY name");
    if (rows.failed()) return common::Result<std::vector<Requirement>>::err(rows.error());
    std::vector<Requirement> out;
    for (const auto& row : rows.value()) out.push_back(fromRequirementRow(row));
    return common::Result<std::vector<Requirement>>::ok(std::move(out));
}

common::Result<void> RequirementDao::update(const Requirement& r) {
    return updateById(db_.handle(), "requirements", kReqCols, requirementValues(r),
                      r.id);
}

common::Result<void> RequirementDao::softDelete(const std::string& id) {
    return softDeleteRow(db_.handle(), "requirements", id);
}

common::Result<std::vector<Requirement>> RequirementDao::findByFilters(const EntityFilter& f) {
    return queryFiltered<Requirement>(db_.handle(), "requirements", kReqCols, f,
                                      &fromRequirementRow);
}

common::Result<std::vector<Requirement>> RequirementDao::search(const std::string& text) {
    EntityFilter f;
    f.text = text;
    return findByFilters(f);
}

// ---------------------------------------------------------------------------
// DesignItemDao
// ---------------------------------------------------------------------------
common::Result<void> DesignItemDao::create(const DesignItem& d) {
    return insertInto(db_.handle(), "design_items", kDesignCols, designValues(d));
}

common::Result<std::optional<DesignItem>> DesignItemDao::findById(const std::string& id) {
    auto rows = selectRows(db_.handle(), "design_items", kDesignCols, "id=?", {id});
    if (rows.failed()) return common::Result<std::optional<DesignItem>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<DesignItem>>::ok(std::nullopt);
    return common::Result<std::optional<DesignItem>>::ok(fromDesignRow(rows.value().front()));
}

common::Result<std::optional<DesignItem>> DesignItemDao::findByExternalId(const std::string& extId) {
    auto rows = selectRows(db_.handle(), "design_items", kDesignCols, "external_id=?", {extId});
    if (rows.failed()) return common::Result<std::optional<DesignItem>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<DesignItem>>::ok(std::nullopt);
    return common::Result<std::optional<DesignItem>>::ok(fromDesignRow(rows.value().front()));
}

common::Result<std::vector<DesignItem>> DesignItemDao::findAll() {
    auto rows = selectRows(db_.handle(), "design_items", kDesignCols, "status != 'Obsolete'", {}, " ORDER BY name");
    if (rows.failed()) return common::Result<std::vector<DesignItem>>::err(rows.error());
    std::vector<DesignItem> out;
    for (const auto& row : rows.value()) out.push_back(fromDesignRow(row));
    return common::Result<std::vector<DesignItem>>::ok(std::move(out));
}

common::Result<void> DesignItemDao::update(const DesignItem& d) {
    return updateById(db_.handle(), "design_items", kDesignCols, designValues(d),
                      d.id);
}

common::Result<void> DesignItemDao::softDelete(const std::string& id) {
    return softDeleteRow(db_.handle(), "design_items", id);
}

common::Result<std::vector<DesignItem>> DesignItemDao::findByFilters(const EntityFilter& f) {
    return queryFiltered<DesignItem>(db_.handle(), "design_items", kDesignCols, f,
                                     &fromDesignRow);
}

common::Result<std::vector<DesignItem>> DesignItemDao::search(const std::string& text) {
    EntityFilter f;
    f.text = text;
    return findByFilters(f);
}

// ---------------------------------------------------------------------------
// InterfaceDao
// ---------------------------------------------------------------------------
common::Result<void> InterfaceDao::create(const InterfaceDef& i) {
    return insertInto(db_.handle(), "interfaces", kInterfaceCols, interfaceValues(i));
}

common::Result<std::optional<InterfaceDef>> InterfaceDao::findById(const std::string& id) {
    auto rows = selectRows(db_.handle(), "interfaces", kInterfaceCols, "id=?", {id});
    if (rows.failed()) return common::Result<std::optional<InterfaceDef>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<InterfaceDef>>::ok(std::nullopt);
    return common::Result<std::optional<InterfaceDef>>::ok(fromInterfaceRow(rows.value().front()));
}

common::Result<std::optional<InterfaceDef>> InterfaceDao::findByExternalId(const std::string& extId) {
    auto rows = selectRows(db_.handle(), "interfaces", kInterfaceCols, "external_id=?", {extId});
    if (rows.failed()) return common::Result<std::optional<InterfaceDef>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<InterfaceDef>>::ok(std::nullopt);
    return common::Result<std::optional<InterfaceDef>>::ok(fromInterfaceRow(rows.value().front()));
}

common::Result<std::vector<InterfaceDef>> InterfaceDao::findAll() {
    auto rows = selectRows(db_.handle(), "interfaces", kInterfaceCols, "status != 'Obsolete'", {}, " ORDER BY name");
    if (rows.failed()) return common::Result<std::vector<InterfaceDef>>::err(rows.error());
    std::vector<InterfaceDef> out;
    for (const auto& row : rows.value()) out.push_back(fromInterfaceRow(row));
    return common::Result<std::vector<InterfaceDef>>::ok(std::move(out));
}

common::Result<void> InterfaceDao::update(const InterfaceDef& i) {
    return updateById(db_.handle(), "interfaces", kInterfaceCols, interfaceValues(i),
                      i.id);
}

common::Result<void> InterfaceDao::softDelete(const std::string& id) {
    return softDeleteRow(db_.handle(), "interfaces", id);
}

common::Result<std::vector<InterfaceDef>> InterfaceDao::findByFilters(const EntityFilter& f) {
    return queryFiltered<InterfaceDef>(db_.handle(), "interfaces", kInterfaceCols, f,
                                       &fromInterfaceRow);
}

common::Result<std::vector<InterfaceDef>> InterfaceDao::search(const std::string& text) {
    EntityFilter f;
    f.text = text;
    return findByFilters(f);
}

// ---------------------------------------------------------------------------
// TestCaseDao
// ---------------------------------------------------------------------------
common::Result<void> TestCaseDao::create(const TestCase& t) {
    return insertInto(db_.handle(), "test_cases", kTestCaseCols, testCaseValues(t));
}

common::Result<std::optional<TestCase>> TestCaseDao::findById(const std::string& id) {
    auto rows = selectRows(db_.handle(), "test_cases", kTestCaseCols, "id=?", {id});
    if (rows.failed()) return common::Result<std::optional<TestCase>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<TestCase>>::ok(std::nullopt);
    return common::Result<std::optional<TestCase>>::ok(fromTestCaseRow(rows.value().front()));
}

common::Result<std::optional<TestCase>> TestCaseDao::findByExternalId(const std::string& extId) {
    auto rows = selectRows(db_.handle(), "test_cases", kTestCaseCols, "external_id=?", {extId});
    if (rows.failed()) return common::Result<std::optional<TestCase>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<TestCase>>::ok(std::nullopt);
    return common::Result<std::optional<TestCase>>::ok(fromTestCaseRow(rows.value().front()));
}

common::Result<std::vector<TestCase>> TestCaseDao::findAll() {
    auto rows = selectRows(db_.handle(), "test_cases", kTestCaseCols, "status != 'Obsolete'", {}, " ORDER BY name");
    if (rows.failed()) return common::Result<std::vector<TestCase>>::err(rows.error());
    std::vector<TestCase> out;
    for (const auto& row : rows.value()) out.push_back(fromTestCaseRow(row));
    return common::Result<std::vector<TestCase>>::ok(std::move(out));
}

common::Result<void> TestCaseDao::update(const TestCase& t) {
    return updateById(db_.handle(), "test_cases", kTestCaseCols, testCaseValues(t),
                      t.id);
}

common::Result<void> TestCaseDao::softDelete(const std::string& id) {
    return softDeleteRow(db_.handle(), "test_cases", id);
}

common::Result<std::vector<TestCase>> TestCaseDao::findByFilters(const EntityFilter& f) {
    return queryFiltered<TestCase>(db_.handle(), "test_cases", kTestCaseCols, f,
                                   &fromTestCaseRow);
}

common::Result<std::vector<TestCase>> TestCaseDao::search(const std::string& text) {
    EntityFilter f;
    f.text = text;
    return findByFilters(f);
}

// ---------------------------------------------------------------------------
// HazardDao
// ---------------------------------------------------------------------------
common::Result<void> HazardDao::create(const Hazard& h) {
    return insertInto(db_.handle(), "hazards", kHazardCols, hazardValues(h));
}

common::Result<std::optional<Hazard>> HazardDao::findById(const std::string& id) {
    auto rows = selectRows(db_.handle(), "hazards", kHazardCols, "id=?", {id});
    if (rows.failed()) return common::Result<std::optional<Hazard>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<Hazard>>::ok(std::nullopt);
    return common::Result<std::optional<Hazard>>::ok(fromHazardRow(rows.value().front()));
}

common::Result<std::optional<Hazard>> HazardDao::findByExternalId(const std::string& extId) {
    auto rows = selectRows(db_.handle(), "hazards", kHazardCols, "external_id=?", {extId});
    if (rows.failed()) return common::Result<std::optional<Hazard>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<Hazard>>::ok(std::nullopt);
    return common::Result<std::optional<Hazard>>::ok(fromHazardRow(rows.value().front()));
}

common::Result<std::vector<Hazard>> HazardDao::findAll() {
    auto rows = selectRows(db_.handle(), "hazards", kHazardCols, "status != 'Obsolete'", {}, " ORDER BY name");
    if (rows.failed()) return common::Result<std::vector<Hazard>>::err(rows.error());
    std::vector<Hazard> out;
    for (const auto& row : rows.value()) out.push_back(fromHazardRow(row));
    return common::Result<std::vector<Hazard>>::ok(std::move(out));
}

common::Result<void> HazardDao::update(const Hazard& h) {
    return updateById(db_.handle(), "hazards", kHazardCols, hazardValues(h), h.id);
}

common::Result<void> HazardDao::softDelete(const std::string& id) {
    return softDeleteRow(db_.handle(), "hazards", id);
}

common::Result<std::vector<Hazard>> HazardDao::findByFilters(const EntityFilter& f) {
    return queryFiltered<Hazard>(db_.handle(), "hazards", kHazardCols, f, &fromHazardRow);
}

common::Result<std::vector<Hazard>> HazardDao::search(const std::string& text) {
    EntityFilter f;
    f.text = text;
    return findByFilters(f);
}

// ---------------------------------------------------------------------------
// DecisionDao
// ---------------------------------------------------------------------------
common::Result<void> DecisionDao::create(const Decision& d) {
    return insertInto(db_.handle(), "decisions", kDecisionCols, decisionValues(d));
}

common::Result<std::optional<Decision>> DecisionDao::findById(const std::string& id) {
    auto rows = selectRows(db_.handle(), "decisions", kDecisionCols, "id=?", {id});
    if (rows.failed()) return common::Result<std::optional<Decision>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<Decision>>::ok(std::nullopt);
    return common::Result<std::optional<Decision>>::ok(fromDecisionRow(rows.value().front()));
}

common::Result<std::optional<Decision>> DecisionDao::findByExternalId(const std::string& extId) {
    auto rows = selectRows(db_.handle(), "decisions", kDecisionCols, "external_id=?", {extId});
    if (rows.failed()) return common::Result<std::optional<Decision>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<Decision>>::ok(std::nullopt);
    return common::Result<std::optional<Decision>>::ok(fromDecisionRow(rows.value().front()));
}

common::Result<std::vector<Decision>> DecisionDao::findAll() {
    auto rows = selectRows(db_.handle(), "decisions", kDecisionCols, "status != 'Obsolete'", {}, " ORDER BY name");
    if (rows.failed()) return common::Result<std::vector<Decision>>::err(rows.error());
    std::vector<Decision> out;
    for (const auto& row : rows.value()) out.push_back(fromDecisionRow(row));
    return common::Result<std::vector<Decision>>::ok(std::move(out));
}

common::Result<void> DecisionDao::update(const Decision& d) {
    return updateById(db_.handle(), "decisions", kDecisionCols, decisionValues(d),
                      d.id);
}

common::Result<void> DecisionDao::softDelete(const std::string& id) {
    return softDeleteRow(db_.handle(), "decisions", id);
}

common::Result<std::vector<Decision>> DecisionDao::findByFilters(const EntityFilter& f) {
    return queryFiltered<Decision>(db_.handle(), "decisions", kDecisionCols, f,
                                   &fromDecisionRow);
}

common::Result<std::vector<Decision>> DecisionDao::search(const std::string& text) {
    EntityFilter f;
    f.text = text;
    return findByFilters(f);
}

// ---------------------------------------------------------------------------
// AssumptionDao
// ---------------------------------------------------------------------------
common::Result<void> AssumptionDao::create(const Assumption& a) {
    return insertInto(db_.handle(), "assumptions", kAssumptionCols, assumptionValues(a));
}

common::Result<std::optional<Assumption>> AssumptionDao::findById(const std::string& id) {
    auto rows = selectRows(db_.handle(), "assumptions", kAssumptionCols, "id=?", {id});
    if (rows.failed()) return common::Result<std::optional<Assumption>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<Assumption>>::ok(std::nullopt);
    return common::Result<std::optional<Assumption>>::ok(fromAssumptionRow(rows.value().front()));
}

common::Result<std::optional<Assumption>> AssumptionDao::findByExternalId(const std::string& extId) {
    auto rows = selectRows(db_.handle(), "assumptions", kAssumptionCols, "external_id=?", {extId});
    if (rows.failed()) return common::Result<std::optional<Assumption>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<Assumption>>::ok(std::nullopt);
    return common::Result<std::optional<Assumption>>::ok(fromAssumptionRow(rows.value().front()));
}

common::Result<std::vector<Assumption>> AssumptionDao::findAll() {
    auto rows = selectRows(db_.handle(), "assumptions", kAssumptionCols, "status != 'Obsolete'", {}, " ORDER BY name");
    if (rows.failed()) return common::Result<std::vector<Assumption>>::err(rows.error());
    std::vector<Assumption> out;
    for (const auto& row : rows.value()) out.push_back(fromAssumptionRow(row));
    return common::Result<std::vector<Assumption>>::ok(std::move(out));
}

common::Result<void> AssumptionDao::update(const Assumption& a) {
    return updateById(db_.handle(), "assumptions", kAssumptionCols,
                      assumptionValues(a), a.id);
}

common::Result<void> AssumptionDao::softDelete(const std::string& id) {
    return softDeleteRow(db_.handle(), "assumptions", id);
}

common::Result<std::vector<Assumption>> AssumptionDao::findByFilters(const EntityFilter& f) {
    return queryFiltered<Assumption>(db_.handle(), "assumptions", kAssumptionCols, f,
                                     &fromAssumptionRow);
}

common::Result<std::vector<Assumption>> AssumptionDao::search(const std::string& text) {
    EntityFilter f;
    f.text = text;
    return findByFilters(f);
}

// ---------------------------------------------------------------------------
// TraceLinkDao
// ---------------------------------------------------------------------------
common::Result<void> TraceLinkDao::create(const TraceLink& link) {
    return insertInto(db_.handle(), "trace_links", kLinkCols, linkValues(link));
}

common::Result<std::optional<TraceLink>> TraceLinkDao::findById(const std::string& id) {
    auto rows = selectRows(db_.handle(), "trace_links", kLinkCols, "id=?", {id});
    if (rows.failed()) return common::Result<std::optional<TraceLink>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<TraceLink>>::ok(std::nullopt);
    return common::Result<std::optional<TraceLink>>::ok(fromLinkRow(rows.value().front()));
}

common::Result<std::vector<TraceLink>> TraceLinkDao::findAll() {
    auto rows = selectRows(db_.handle(), "trace_links", kLinkCols, "", {}, " ORDER BY id");
    if (rows.failed()) return common::Result<std::vector<TraceLink>>::err(rows.error());
    std::vector<TraceLink> out;
    for (const auto& row : rows.value()) out.push_back(fromLinkRow(row));
    return common::Result<std::vector<TraceLink>>::ok(std::move(out));
}

common::Result<std::vector<TraceLink>> TraceLinkDao::findBySource(const std::string& sourceType,
                                                                  const std::string& sourceId) {
    auto rows = selectRows(db_.handle(), "trace_links", kLinkCols,
                           "source_type=? AND source_id=?", {sourceType, sourceId},
                           " ORDER BY id");
    if (rows.failed()) return common::Result<std::vector<TraceLink>>::err(rows.error());
    std::vector<TraceLink> out;
    for (const auto& row : rows.value()) out.push_back(fromLinkRow(row));
    return common::Result<std::vector<TraceLink>>::ok(std::move(out));
}

common::Result<std::vector<TraceLink>> TraceLinkDao::findByTarget(const std::string& targetType,
                                                                  const std::string& targetId) {
    auto rows = selectRows(db_.handle(), "trace_links", kLinkCols,
                           "target_type=? AND target_id=?", {targetType, targetId},
                           " ORDER BY id");
    if (rows.failed()) return common::Result<std::vector<TraceLink>>::err(rows.error());
    std::vector<TraceLink> out;
    for (const auto& row : rows.value()) out.push_back(fromLinkRow(row));
    return common::Result<std::vector<TraceLink>>::ok(std::move(out));
}

common::Result<bool> TraceLinkDao::existsActive(const std::string& sourceType,
                                                const std::string& sourceId,
                                                const std::string& targetType,
                                                const std::string& targetId,
                                                const std::string& relation) {
    auto rows = selectRows(db_.handle(), "trace_links", {"id"},
                           "source_type=? AND source_id=? AND target_type=? AND "
                           "target_id=? AND relation=? AND status='Active'",
                           {sourceType, sourceId, targetType, targetId, relation});
    if (rows.failed()) return common::Result<bool>::err(rows.error());
    return common::Result<bool>::ok(!rows.value().empty());
}

common::Result<void> TraceLinkDao::update(const TraceLink& link) {
    return updateById(db_.handle(), "trace_links", kLinkCols, linkValues(link),
                      link.id);
}

common::Result<void> TraceLinkDao::softDelete(const std::string& id) {
    // Mark the link Superseded (kept for history rather than hard-deleted).
    return execBinds(db_.handle(),
                     "UPDATE trace_links SET status='Superseded' WHERE id=?;", {id});
}

// ---------------------------------------------------------------------------
// ScenarioDao
// ---------------------------------------------------------------------------
common::Result<void> ScenarioDao::create(const Scenario& s) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO scenarios (id, name, description, status) VALUES (?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<void>::err("prepare failed: " +
                                         std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, s.id);
    bindText(stmt, 2, s.name);
    bindText(stmt, 3, s.description);
    bindText(stmt, 4, s.status);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err("insert scenario failed: " +
                                         std::string(sqlite3_errmsg(db_.handle())));
    }
    return common::Result<void>::ok();
}

common::Result<std::vector<Scenario>> ScenarioDao::findAll() {
    std::vector<Scenario> out;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, name, description, status FROM scenarios ORDER BY name;";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::vector<Scenario>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Scenario s;
        s.id = columnText(stmt, 0);
        s.name = columnText(stmt, 1);
        s.description = columnText(stmt, 2);
        s.status = columnText(stmt, 3);
        out.push_back(std::move(s));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<Scenario>>::ok(std::move(out));
}

}  // namespace lodestar::persistence
