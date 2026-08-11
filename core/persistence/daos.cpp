// core/persistence/daos.cpp
#include "core/persistence/daos.h"

#include <sqlite3.h>

namespace lodestar::persistence {

namespace {

// Binds a string parameter at the given 1-based index.
void bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()),
                      SQLITE_TRANSIENT);
}

// Reads a TEXT column as std::string (NULL -> empty).
std::string columnText(sqlite3_stmt* stmt, int col) {
    const unsigned char* text = sqlite3_column_text(stmt, col);
    return text ? reinterpret_cast<const char*>(text) : std::string();
}

}  // namespace

// ---------------------------------------------------------------------------
// RequirementDao
// ---------------------------------------------------------------------------
common::Result<void> RequirementDao::create(const Requirement& r) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO requirements (id, name, description, status) VALUES (?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<void>::err("prepare failed: " +
                                         std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, r.id);
    bindText(stmt, 2, r.name);
    bindText(stmt, 3, r.description);
    bindText(stmt, 4, r.status);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err("insert requirement failed: " +
                                         std::string(sqlite3_errmsg(db_.handle())));
    }
    return common::Result<void>::ok();
}

common::Result<std::vector<Requirement>> RequirementDao::findAll() {
    std::vector<Requirement> out;
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, name, description, status FROM requirements ORDER BY name;";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::vector<Requirement>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Requirement r;
        r.id = columnText(stmt, 0);
        r.name = columnText(stmt, 1);
        r.description = columnText(stmt, 2);
        r.status = columnText(stmt, 3);
        out.push_back(std::move(r));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<Requirement>>::ok(std::move(out));
}

common::Result<std::optional<Requirement>> RequirementDao::findById(const std::string& id) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, name, description, status FROM requirements WHERE id = ?;";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::optional<Requirement>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, id);
    std::optional<Requirement> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Requirement r;
        r.id = columnText(stmt, 0);
        r.name = columnText(stmt, 1);
        r.description = columnText(stmt, 2);
        r.status = columnText(stmt, 3);
        result = std::move(r);
    }
    sqlite3_finalize(stmt);
    return common::Result<std::optional<Requirement>>::ok(std::move(result));
}

// ---------------------------------------------------------------------------
// DesignItemDao
// ---------------------------------------------------------------------------
common::Result<void> DesignItemDao::create(const DesignItem& d) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO design_items (id, name, description) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<void>::err("prepare failed: " +
                                         std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, d.id);
    bindText(stmt, 2, d.name);
    bindText(stmt, 3, d.description);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err("insert design_item failed: " +
                                         std::string(sqlite3_errmsg(db_.handle())));
    }
    return common::Result<void>::ok();
}

common::Result<std::vector<DesignItem>> DesignItemDao::findAll() {
    std::vector<DesignItem> out;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, name, description FROM design_items ORDER BY name;";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::vector<DesignItem>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DesignItem d;
        d.id = columnText(stmt, 0);
        d.name = columnText(stmt, 1);
        d.description = columnText(stmt, 2);
        out.push_back(std::move(d));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<DesignItem>>::ok(std::move(out));
}

// ---------------------------------------------------------------------------
// InterfaceDao
// ---------------------------------------------------------------------------
common::Result<void> InterfaceDao::create(const InterfaceDef& i) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO interfaces (id, name, description) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<void>::err("prepare failed: " +
                                         std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, i.id);
    bindText(stmt, 2, i.name);
    bindText(stmt, 3, i.description);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err("insert interface failed: " +
                                         std::string(sqlite3_errmsg(db_.handle())));
    }
    return common::Result<void>::ok();
}

common::Result<std::vector<InterfaceDef>> InterfaceDao::findAll() {
    std::vector<InterfaceDef> out;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, name, description FROM interfaces ORDER BY name;";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::vector<InterfaceDef>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        InterfaceDef i;
        i.id = columnText(stmt, 0);
        i.name = columnText(stmt, 1);
        i.description = columnText(stmt, 2);
        out.push_back(std::move(i));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<InterfaceDef>>::ok(std::move(out));
}

// ---------------------------------------------------------------------------
// TestCaseDao
// ---------------------------------------------------------------------------
common::Result<void> TestCaseDao::create(const TestCase& t) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO test_cases (id, name, description, status) VALUES (?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<void>::err("prepare failed: " +
                                         std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, t.id);
    bindText(stmt, 2, t.name);
    bindText(stmt, 3, t.description);
    bindText(stmt, 4, t.status);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err("insert test_case failed: " +
                                         std::string(sqlite3_errmsg(db_.handle())));
    }
    return common::Result<void>::ok();
}

common::Result<std::vector<TestCase>> TestCaseDao::findAll() {
    std::vector<TestCase> out;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, name, description, status FROM test_cases ORDER BY name;";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::vector<TestCase>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TestCase t;
        t.id = columnText(stmt, 0);
        t.name = columnText(stmt, 1);
        t.description = columnText(stmt, 2);
        t.status = columnText(stmt, 3);
        out.push_back(std::move(t));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<TestCase>>::ok(std::move(out));
}

common::Result<std::optional<TestCase>> TestCaseDao::findById(const std::string& id) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, name, description, status FROM test_cases WHERE id = ?;";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::optional<TestCase>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, id);
    std::optional<TestCase> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        TestCase t;
        t.id = columnText(stmt, 0);
        t.name = columnText(stmt, 1);
        t.description = columnText(stmt, 2);
        t.status = columnText(stmt, 3);
        result = std::move(t);
    }
    sqlite3_finalize(stmt);
    return common::Result<std::optional<TestCase>>::ok(std::move(result));
}

// ---------------------------------------------------------------------------
// TraceLinkDao
// ---------------------------------------------------------------------------
common::Result<void> TraceLinkDao::create(const TraceLink& link) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO trace_links (id, source_type, source_id, target_type, target_id, relation) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<void>::err("prepare failed: " +
                                         std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, link.id);
    bindText(stmt, 2, link.sourceType);
    bindText(stmt, 3, link.sourceId);
    bindText(stmt, 4, link.targetType);
    bindText(stmt, 5, link.targetId);
    bindText(stmt, 6, link.relation);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err("insert trace_link failed: " +
                                         std::string(sqlite3_errmsg(db_.handle())));
    }
    return common::Result<void>::ok();
}

namespace {
common::Result<std::vector<TraceLink>> queryLinks(sqlite3* db, const char* sql,
                                                  const std::string& a,
                                                  const std::string& b) {
    std::vector<TraceLink> out;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::vector<TraceLink>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db)));
    }
    bindText(stmt, 1, a);
    bindText(stmt, 2, b);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TraceLink l;
        l.id = columnText(stmt, 0);
        l.sourceType = columnText(stmt, 1);
        l.sourceId = columnText(stmt, 2);
        l.targetType = columnText(stmt, 3);
        l.targetId = columnText(stmt, 4);
        l.relation = columnText(stmt, 5);
        out.push_back(std::move(l));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<TraceLink>>::ok(std::move(out));
}
}  // namespace

common::Result<std::vector<TraceLink>> TraceLinkDao::findAll() {
    const char* sql =
        "SELECT id, source_type, source_id, target_type, target_id, relation "
        "FROM trace_links ORDER BY id;";
    return queryLinks(db_.handle(), sql, std::string(), std::string());
}

common::Result<std::vector<TraceLink>> TraceLinkDao::findBySource(const std::string& sourceType,
                                                                  const std::string& sourceId) {
    const char* sql =
        "SELECT id, source_type, source_id, target_type, target_id, relation "
        "FROM trace_links WHERE source_type = ? AND source_id = ? ORDER BY id;";
    return queryLinks(db_.handle(), sql, sourceType, sourceId);
}

common::Result<std::vector<TraceLink>> TraceLinkDao::findByTarget(const std::string& targetType,
                                                                  const std::string& targetId) {
    const char* sql =
        "SELECT id, source_type, source_id, target_type, target_id, relation "
        "FROM trace_links WHERE target_type = ? AND target_id = ? ORDER BY id;";
    return queryLinks(db_.handle(), sql, targetType, targetId);
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
