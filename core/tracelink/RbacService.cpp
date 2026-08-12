// core/tracelink/RbacService.cpp
// WP-4: User roles + permissions (RBAC) on entities/links.

#include "core/tracelink/RbacService.h"

#include <string>
#include <vector>

#include <sqlite3.h>

#include "core/common/Uuid.h"

namespace lodestar::tracelink {

using lodestar::common::newUuid;

namespace {

// Binds a text parameter onto a prepared statement.
void bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()),
                      SQLITE_TRANSIENT);
}

// Executes a parameterized statement that returns no rows.
common::Result<void> exec(sqlite3* db, const std::string& sql,
                          const std::vector<std::string>& params) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<void>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db)));
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

// Returns the first column of the first row, or "" if none.
std::string queryScalar(sqlite3* db, const std::string& sql,
                        const std::vector<std::string>& params) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return "";
    }
    for (size_t i = 0; i < params.size(); ++i) {
        bindText(stmt, static_cast<int>(i + 1), params[i]);
    }
    std::string out;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        if (text) out = reinterpret_cast<const char*>(text);
    }
    sqlite3_finalize(stmt);
    return out;
}

bool isValidRole(const std::string& role) {
    return role == "admin" || role == "editor" || role == "reviewer" ||
           role == "viewer";
}

}  // namespace

RbacService::RbacService(persistence::Database& db) : db_(db) {}

common::Result<User> RbacService::createUser(const std::string& username,
                                             const std::string& role) {
    if (username.empty()) {
        return common::Result<User>::err(common::ErrorCode::InvalidArgument,
                                          "username must not be empty");
    }
    if (!isValidRole(role)) {
        return common::Result<User>::err(
            common::ErrorCode::InvalidArgument,
            "invalid role '" + role + "' (expected admin|editor|reviewer|viewer)");
    }

    // Reject a duplicate username.
    std::string existing = queryScalar(
        db_.handle(), "SELECT id FROM users WHERE username=?;",
        std::vector<std::string>{username});
    if (!existing.empty()) {
        return common::Result<User>::err(common::ErrorCode::Duplicate,
                                         "user already exists: " + username);
    }

    User u;
    u.id = newUuid();
    u.username = username;
    u.role = role;

    auto res = exec(db_.handle(),
                    "INSERT INTO users (id, username, role) VALUES (?, ?, ?);",
                    std::vector<std::string>{u.id, u.username, u.role});
    if (res.failed()) {
        return common::Result<User>::err(common::ErrorCode::DatabaseError,
                                         res.error());
    }
    return common::Result<User>::ok(std::move(u));
}

common::Result<void> RbacService::grantPermission(const std::string& userId,
                                                  const std::string& permission,
                                                  const std::string& entityType) {
    if (permission.empty()) {
        return common::Result<void>::err(common::ErrorCode::InvalidArgument,
                                         "permission must not be empty");
    }
    std::string user = queryScalar(db_.handle(), "SELECT id FROM users WHERE id=?;",
                                   std::vector<std::string>{userId});
    if (user.empty()) {
        return common::Result<void>::err(common::ErrorCode::NotFound,
                                         "user not found: " + userId);
    }
    auto res = exec(db_.handle(),
                    "INSERT INTO user_permissions (id, user_id, permission, "
                    "entity_type) VALUES (?, ?, ?, ?);",
                    std::vector<std::string>{newUuid(), userId, permission, entityType});
    if (res.failed()) {
        return common::Result<void>::err(common::ErrorCode::DatabaseError,
                                         res.error());
    }
    return common::Result<void>::ok();
}

common::Result<bool> RbacService::hasPermission(const std::string& userId,
                                                const std::string& permission,
                                                const std::string& entityType) {
    std::string role = queryScalar(db_.handle(), "SELECT role FROM users WHERE id=?;",
                                   std::vector<std::string>{userId});
    if (role.empty()) {
        return common::Result<bool>::err(common::ErrorCode::NotFound,
                                         "user not found: " + userId);
    }
    // The admin role implicitly holds every permission.
    if (role == "admin") {
        return common::Result<bool>::ok(true);
    }
    // A grant scoped to '' (all types) or to the requested entity type counts.
    std::string hit = queryScalar(
        db_.handle(),
        "SELECT id FROM user_permissions WHERE user_id=? AND permission=? AND "
        "(entity_type='' OR entity_type=?) LIMIT 1;",
        std::vector<std::string>{userId, permission, entityType});
    return common::Result<bool>::ok(!hit.empty());
}

common::Result<void> RbacService::requirePermission(const std::string& userId,
                                                    const std::string& permission,
                                                    const std::string& entityType) {
    auto res = hasPermission(userId, permission, entityType);
    if (res.failed()) {
        return common::Result<void>::err(res.errorCode(), res.error());
    }
    if (!res.value()) {
        return common::Result<void>::err(
            common::ErrorCode::ValidationFailed,
            "permission denied: user " + userId + " lacks '" + permission + "'");
    }
    return common::Result<void>::ok();
}

}  // namespace lodestar::tracelink
