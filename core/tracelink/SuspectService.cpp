#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// core/tracelink/SuspectService.cpp
// Phase 10 WP-1 (A1): suspect-link workflow.

#include "core/tracelink/SuspectService.h"

#include <ctime>
#include <set>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "core/common/Uuid.h"
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

common::Result<std::vector<std::vector<std::string>>> query(
    sqlite3* db, const std::string& sql, const std::vector<std::string>& params,
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

SuspectFlag flagFromRow(const std::vector<std::string>& r) {
    SuspectFlag f;
    f.id = r[0];
    f.entityType = r[1];
    f.entityId = r[2];
    f.reason = r[3];
    f.sourceType = r[4];
    f.sourceId = r[5];
    f.createdAt = r[6];
    return f;
}

}  // namespace

SuspectService::SuspectService(persistence::Database& db) : db_(db) {}

common::Result<SuspectFlag> SuspectService::flagSuspect(
    const std::string& entityType, const std::string& entityId,
    const std::string& reason, const std::string& sourceType,
    const std::string& sourceId) {
    SuspectFlag f;
    f.id = newUuid();
    f.entityType = entityType;
    f.entityId = entityId;
    f.reason = reason;
    f.sourceType = sourceType;
    f.sourceId = sourceId;
    f.createdAt = now();

    auto res = exec(db_.handle(),
                    "INSERT INTO suspect_flags (id, entity_type, entity_id, reason, "
                    "source_type, source_id, created_at, cleared_at, cleared_by) "
                    "VALUES (?,?,?,?,?,?,?,'','');",
                    {f.id, f.entityType, f.entityId, f.reason, f.sourceType, f.sourceId,
                     f.createdAt});
    if (res.failed()) return common::Result<SuspectFlag>::err(res.error());
    return common::Result<SuspectFlag>::ok(std::move(f));
}

common::Result<std::vector<SuspectFlag>> SuspectService::suspectQueue() {
    auto rows = query(db_.handle(),
                      "SELECT id, entity_type, entity_id, reason, source_type, "
                      "source_id, created_at FROM suspect_flags "
                      "WHERE cleared_at='' "
                      "ORDER BY created_at DESC, rowid DESC;",
                      {}, 7);
    if (rows.failed()) {
        return common::Result<std::vector<SuspectFlag>>::err(rows.error());
    }
    std::vector<SuspectFlag> out;
    for (const auto& r : rows.value()) out.push_back(flagFromRow(r));
    return common::Result<std::vector<SuspectFlag>>::ok(std::move(out));
}

common::Result<bool> SuspectService::isSuspect(const std::string& entityType,
                                               const std::string& entityId) {
    auto rows = query(db_.handle(),
                      "SELECT id FROM suspect_flags "
                      "WHERE entity_type=? AND entity_id=? AND cleared_at='' LIMIT 1;",
                      {entityType, entityId}, 1);
    if (rows.failed()) return common::Result<bool>::err(rows.error());
    return common::Result<bool>::ok(!rows.value().empty());
}

common::Result<void> SuspectService::clearSuspect(const std::string& flagId,
                                                  const std::string& clearedBy) {
    auto res = exec(db_.handle(),
                    "UPDATE suspect_flags SET cleared_at=?, cleared_by=? "
                    "WHERE id=? AND cleared_at='';",
                    {now(), clearedBy, flagId});
    if (res.failed()) return common::Result<void>::err(res.error());
    return common::Result<void>::ok();
}

common::Result<std::vector<SuspectFlag>> SuspectService::autoFlagDownstream(
    const std::string& entityType, const std::string& entityId,
    const std::string& reason) {
    auto typeOpt = entityTypeFromString(entityType);
    if (!typeOpt) {
        return common::Result<std::vector<SuspectFlag>>::err(
            common::ErrorCode::InvalidArgument, "invalid entity type: " + entityType);
    }

    TraceLinkService svc(db_);
    std::vector<SuspectFlag> flags;
    std::set<std::string> visited;  // "type:id" guard against cycles
    std::vector<std::pair<std::string, std::string>> frontier;
    frontier.emplace_back(entityType, entityId);

    while (!frontier.empty()) {
        auto [type, id] = frontier.back();
        frontier.pop_back();
        std::string key = type + ":" + id;
        if (visited.count(key)) continue;
        visited.insert(key);

        auto typeE = entityTypeFromString(type);
        if (!typeE) continue;
        auto links = svc.linksTo(*typeE, id);
        if (links.failed()) {
            return common::Result<std::vector<SuspectFlag>>::err(links.error());
        }
        for (const auto& link : links.value()) {
            if (link.status != "Active") continue;
            // The source of a link whose target is the changed entity is a
            // downstream artifact that depends on it.
            const std::string& srcType = toString(link.sourceType);
            const std::string& srcId = link.sourceId;
            std::string srcKey = srcType + ":" + srcId;
            if (visited.count(srcKey)) continue;

            auto flag = flagSuspect(srcType, srcId, reason, entityType, entityId);
            if (flag.failed()) {
                return common::Result<std::vector<SuspectFlag>>::err(flag.error());
            }
            flags.push_back(flag.value());
            frontier.emplace_back(srcType, srcId);
        }
    }

    return common::Result<std::vector<SuspectFlag>>::ok(std::move(flags));
}

common::Result<std::vector<std::string>> SuspectService::suspectLinks() {
    TraceLinkService svc(db_);
    auto links = svc.allLinks();
    if (links.failed()) {
        return common::Result<std::vector<std::string>>::err(links.error());
    }
    std::vector<std::string> out;
    for (const auto& link : links.value()) {
        if (link.status != "Active") continue;
        auto src = isSuspect(toString(link.sourceType), link.sourceId);
        if (src.failed()) return common::Result<std::vector<std::string>>::err(src.error());
        auto tgt = isSuspect(toString(link.targetType), link.targetId);
        if (tgt.failed()) return common::Result<std::vector<std::string>>::err(tgt.error());
        if (src.value() || tgt.value()) out.push_back(link.id);
    }
    return common::Result<std::vector<std::string>>::ok(std::move(out));
}

}  // namespace lodestar::tracelink
