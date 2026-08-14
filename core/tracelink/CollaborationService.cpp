// core/tracelink/CollaborationService.cpp
// Gap-Fill TraceLink 3.2: real-time multi-user collaboration.

#include "core/tracelink/CollaborationService.h"

#include <sqlite3.h>

#include "core/common/Time.h"
#include "core/common/Uuid.h"

namespace lodestar::tracelink {

using lodestar::common::newUuid;
using lodestar::common::nowIso;

namespace {

void bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()),
                      SQLITE_TRANSIENT);
}

std::string colText(sqlite3_stmt* stmt, int col) {
    const unsigned char* t = sqlite3_column_text(stmt, col);
    return t ? reinterpret_cast<const char*>(t) : std::string();
}

int colInt(sqlite3_stmt* stmt, int col) { return sqlite3_column_int(stmt, col); }

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

int queryInt(sqlite3* db, const std::string& sql,
             const std::vector<std::string>& params) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    for (size_t i = 0; i < params.size(); ++i) {
        bindText(stmt, static_cast<int>(i + 1), params[i]);
    }
    int v = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) v = colInt(stmt, 0);
    sqlite3_finalize(stmt);
    return v;
}

}  // namespace

CollaborationService::CollaborationService(persistence::Database& db)
    : db_(db) {}

int CollaborationService::currentVersionInternal(const std::string& entityId) {
    return queryInt(db_.handle(),
                    "SELECT COALESCE(MAX(version),0) FROM collab_operation_log "
                    "WHERE entity_id=?;",
                    {entityId});
}

common::Result<int> CollaborationService::currentVersion(
    const std::string& entityId) {
    if (entityId.empty()) {
        return common::Result<int>::err(common::ErrorCode::InvalidArgument,
                                        "entityId must not be empty");
    }
    return common::Result<int>::ok(currentVersionInternal(entityId));
}

common::Result<CollabOperation> CollaborationService::recordOperation(
    const std::string& entityType, const std::string& entityId,
    const std::string& op, const std::string& actor,
    const std::string& payload) {
    if (entityId.empty()) {
        return common::Result<CollabOperation>::err(
            common::ErrorCode::InvalidArgument, "entityId must not be empty");
    }
    if (op != "create" && op != "update" && op != "delete") {
        return common::Result<CollabOperation>::err(
            common::ErrorCode::InvalidArgument, "invalid operation: " + op);
    }
    const int version = currentVersionInternal(entityId) + 1;
    CollabOperation o;
    o.id = newUuid();
    o.entityType = entityType;
    o.entityId = entityId;
    o.op = op;
    o.actor = actor;
    o.version = version;
    o.payload = payload;
    o.createdAt = nowIso();

    auto begin = db_.beginImmediate();
    if (begin.failed()) {
        return common::Result<CollabOperation>::err(begin.error());
    }
    auto ins = exec(db_.handle(),
                    "INSERT INTO collab_operation_log "
                    "(id, entity_type, entity_id, op, actor, version, payload, "
                    " created_at) VALUES (?,?,?,?,?,?,?,?);",
                    {o.id, o.entityType, o.entityId, o.op, o.actor,
                     std::to_string(o.version), o.payload, o.createdAt});
    if (ins.failed()) {
        db_.rollback();
        return common::Result<CollabOperation>::err(ins.error());
    }
    // Update the actor's vector element (upsert).
    exec(db_.handle(),
         "INSERT INTO collab_vector (id, entity_id, actor, version) "
         "VALUES (?,?,?,?) "
         "ON CONFLICT(entity_id, actor) DO UPDATE SET version=excluded.version;",
         {newUuid(), o.entityId, o.actor, std::to_string(o.version)});
    db_.commit();
    return common::Result<CollabOperation>::ok(std::move(o));
}

common::Result<EditResult> CollaborationService::optimisticUpdate(
    const std::string& entityType, const std::string& entityId,
    const std::string& actor, const std::string& payload, int baseVersion) {
    if (entityId.empty()) {
        return common::Result<EditResult>::err(
            common::ErrorCode::InvalidArgument, "entityId must not be empty");
    }
    const int current = currentVersionInternal(entityId);
    EditResult out;
    out.currentVersion = current;

    if (baseVersion != current) {
        out.status = EditStatus::NotCurrent;
        auto vec = vectorFor(entityId);
        if (vec.isOk()) out.currentVector = vec.value();
        return common::Result<EditResult>::ok(out);
    }

    auto op = recordOperation(entityType, entityId, "update", actor, payload);
    if (op.failed()) {
        return common::Result<EditResult>::err(op.error());
    }
    out.status = EditStatus::Applied;
    out.currentVersion = op.value().version;
    auto vec = vectorFor(entityId);
    if (vec.isOk()) out.currentVector = vec.value();
    return common::Result<EditResult>::ok(out);
}

common::Result<std::vector<VectorElement>> CollaborationService::vectorFor(
    const std::string& entityId) {
    std::vector<VectorElement> out;
    sqlite3_stmt* stmt = nullptr;
    const std::string sql =
        "SELECT actor, version FROM collab_vector WHERE entity_id=? "
        "ORDER BY actor;";
    if (sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        return common::Result<std::vector<VectorElement>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, entityId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        VectorElement e;
        e.actor = colText(stmt, 0);
        e.version = colInt(stmt, 1);
        out.push_back(std::move(e));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<VectorElement>>::ok(std::move(out));
}

common::Result<std::vector<CollabOperation>> CollaborationService::changesSince(
    const std::string& entityId, int afterVersion) {
    std::vector<CollabOperation> out;
    sqlite3_stmt* stmt = nullptr;
    const std::string sql =
        "SELECT id, entity_type, entity_id, op, actor, version, payload, "
        " created_at FROM collab_operation_log "
        "WHERE entity_id=? AND version>? ORDER BY version;";
    if (sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        return common::Result<std::vector<CollabOperation>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, entityId);
    bindText(stmt, 2, std::to_string(afterVersion));
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CollabOperation o;
        o.id = colText(stmt, 0);
        o.entityType = colText(stmt, 1);
        o.entityId = colText(stmt, 2);
        o.op = colText(stmt, 3);
        o.actor = colText(stmt, 4);
        o.version = colInt(stmt, 5);
        o.payload = colText(stmt, 6);
        o.createdAt = colText(stmt, 7);
        out.push_back(std::move(o));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<CollabOperation>>::ok(std::move(out));
}

common::Result<EditStatus> CollaborationService::merge(
    const std::string& entityId, const std::string& actor,
    const std::vector<VectorElement>& remote) {
    if (entityId.empty()) {
        return common::Result<EditStatus>::err(common::ErrorCode::InvalidArgument,
                                               "entityId must not be empty");
    }
    auto local = vectorFor(entityId);
    if (local.failed()) {
        return common::Result<EditStatus>::err(local.error());
    }
    // Conflict: a remote element advanced the entity beyond what this actor has
    // seen, AND this actor also has a pending change. For simplicity, a merge
    // conflicts when the remote reports a higher version for any actor than the
    // local actor's own version and the local has not incorporated it.
    for (const auto& r : remote) {
        if (r.actor == actor) continue;  // our own element
        // If a remote actor's version exceeds the local actor's seen version,
        // there is a concurrent divergence to resolve.
        int localActorVersion = 0;
        for (const auto& l : local.value()) {
            if (l.actor == actor) localActorVersion = l.version;
        }
        if (r.version > localActorVersion && localActorVersion > 0) {
            return common::Result<EditStatus>::ok(EditStatus::Conflict);
        }
    }
    return common::Result<EditStatus>::ok(EditStatus::Applied);
}

}  // namespace lodestar::tracelink
