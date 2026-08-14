// core/tracelink/UserService.cpp
// S2 Phase 1: user accounts with login, roles, permissions and concurrent-edit
// conflict handling. See UserService.h.

#include "core/tracelink/UserService.h"

#include <string>
#include <vector>

#include <sqlite3.h>

#include "core/common/Sha256.h"
#include "core/common/Uuid.h"

namespace lodestar::tracelink {

using lodestar::common::newUuid;
using lodestar::common::sha256Hex;

namespace {

void bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()),
                      SQLITE_TRANSIENT);
}

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

// Iterated key-derivation function (PBKDF2-style): the password is hashed
// repeatedly with the salt, so brute-force of a leaked hash is far more costly
// than a single SHA-256. This is the gap-fill (CC#2) replacement for the
// single-pass hashPassword below.
constexpr int kKdfIterations = 10000;

// Iterated KDF hash: H^iterations(salt || password).
std::string kdfHash(const std::string& salt, const std::string& password) {
    std::string h = salt + password;
    for (int i = 0; i < kKdfIterations; ++i) h = sha256Hex(h);
    return h;
}

// Legacy single-pass hash (kept to verify pre-existing accounts that were
// registered before the iterated KDF shipped).
std::string hashPassword(const std::string& salt, const std::string& password) {
    return sha256Hex(salt + password);
}

// Verify a stored "salt:hash" credential. New accounts use the iterated KDF
// (stored as "iter:salt:hash"); legacy accounts use the single-pass hash
// (stored as "salt:hash"). Returns true on match.
bool verifyPassword(const std::string& stored, const std::string& password) {
    size_t colon = stored.find(':');
    if (colon == std::string::npos) return false;
    std::string salt = stored.substr(0, colon);
    std::string rest = stored.substr(colon + 1);
    if (salt == "iter" && rest.size() > 1) {
        // "iter:salt:hash" -> second ':' separates salt and hash.
        size_t c2 = rest.find(':');
        if (c2 == std::string::npos) return false;
        std::string realSalt = rest.substr(0, c2);
        std::string realHash = rest.substr(c2 + 1);
        return kdfHash(realSalt, password) == realHash;
    }
    // Legacy single-pass.
    return hashPassword(salt, password) == rest;
}

}  // namespace

UserService::UserService(persistence::Database& db)
    : db_(db), rbac_(db), tracelink_(db) {}

common::Result<UserAccount> UserService::registerUser(
    const std::string& username, const std::string& password,
    const std::string& role) {
    if (username.empty()) {
        return common::Result<UserAccount>::err(common::ErrorCode::InvalidArgument,
                                                "username must not be empty");
    }
    if (password.empty()) {
        return common::Result<UserAccount>::err(common::ErrorCode::InvalidArgument,
                                                "password must not be empty");
    }
    if (!isValidRole(role)) {
        return common::Result<UserAccount>::err(
            common::ErrorCode::InvalidArgument,
            "invalid role '" + role + "' (expected admin|editor|reviewer|viewer)");
    }

    std::string existing = queryScalar(
        db_.handle(), "SELECT id FROM users WHERE username=?;",
        std::vector<std::string>{username});
    if (!existing.empty()) {
        return common::Result<UserAccount>::err(common::ErrorCode::Duplicate,
                                                "user already exists: " + username);
    }

    // Iterated salted KDF: store "iter:salt:kdfHash" so the plaintext is never
    // persisted and the stored hash resists offline brute-force. This is the
    // gap-fill (CC#2) upgrade from the single-pass salted SHA-256.
    std::string salt = newUuid();
    std::string stored = "iter:" + salt + ":" + kdfHash(salt, password);

    UserAccount u;
    u.id = newUuid();
    u.username = username;
    u.role = role;

    auto res = exec(db_.handle(),
                    "INSERT INTO users (id, username, role, password_hash) "
                    "VALUES (?, ?, ?, ?);",
                    std::vector<std::string>{u.id, u.username, u.role, stored});
    if (res.failed()) {
        return common::Result<UserAccount>::err(common::ErrorCode::DatabaseError,
                                                res.error());
    }
    return common::Result<UserAccount>::ok(std::move(u));
}

common::Result<std::string> UserService::login(const std::string& username,
                                               const std::string& password) {
    std::string row = queryScalar(
        db_.handle(),
        "SELECT id || '|' || password_hash FROM users WHERE username=?;",
        std::vector<std::string>{username});
    if (row.empty()) {
        return common::Result<std::string>::err(common::ErrorCode::NotFound,
                                                "user not found: " + username);
    }
    size_t sep = row.find('|');
    std::string id = row.substr(0, sep);
    std::string stored = row.substr(sep + 1);
    size_t colon = stored.find(':');
    if (colon == std::string::npos) {
        return common::Result<std::string>::err(common::ErrorCode::Internal,
                                                "malformed stored password hash");
    }
    if (!verifyPassword(stored, password)) {
        return common::Result<std::string>::err(common::ErrorCode::ValidationFailed,
                                                "invalid credentials");
    }

    std::string token = newUuid() + newUuid();
    // Build the expiry modifier explicitly (e.g. "+1 days") so the SQLite
    // datetime() function gets a well-formed modifier.
    std::string modifier = "+" + std::to_string(sessionLifetimeDays_) + " days";
    auto res = exec(db_.handle(),
                    "INSERT INTO sessions (token, user_id, expires_at) "
                    "VALUES (?, ?, datetime('now', ?));",
                    std::vector<std::string>{token, id, modifier});
    if (res.failed()) {
        return common::Result<std::string>::err(common::ErrorCode::DatabaseError,
                                                res.error());
    }
    return common::Result<std::string>::ok(token);
}

common::Result<void> UserService::logout(const std::string& token) {
    auto res = exec(db_.handle(), "DELETE FROM sessions WHERE token=?;",
                    std::vector<std::string>{token});
    if (res.failed()) {
        return common::Result<void>::err(common::ErrorCode::DatabaseError,
                                         res.error());
    }
    return common::Result<void>::ok();
}

void UserService::setSessionLifetimeDays(int days) {
    sessionLifetimeDays_ = days < 1 ? 1 : days;
}

common::Result<void> UserService::expireToken(const std::string& token) {
    if (token.empty()) {
        return common::Result<void>::err(common::ErrorCode::InvalidArgument,
                                         "token must not be empty");
    }
    // Mark the session expired by setting its expiry into the past.
    auto res = exec(db_.handle(),
                    "UPDATE sessions SET expires_at=datetime('now','-1 hour') "
                    "WHERE token=?;",
                    std::vector<std::string>{token});
    if (res.failed()) {
        return common::Result<void>::err(common::ErrorCode::DatabaseError,
                                         res.error());
    }
    return common::Result<void>::ok();
}

common::Result<bool> UserService::isSessionValid(const std::string& token) {
    std::string userId = queryScalar(
        db_.handle(),
        "SELECT user_id FROM sessions WHERE token=? AND expires_at > datetime('now');",
        std::vector<std::string>{token});
    return common::Result<bool>::ok(!userId.empty());
}

common::Result<UserAccount> UserService::currentUser(const std::string& token) {
    std::string userId = queryScalar(
        db_.handle(),
        "SELECT user_id FROM sessions WHERE token=? AND expires_at > datetime('now');",
        std::vector<std::string>{token});
    if (userId.empty()) {
        return common::Result<UserAccount>::err(common::ErrorCode::NotFound,
                                                "invalid or expired session");
    }
    std::string username = queryScalar(
        db_.handle(), "SELECT username FROM users WHERE id=?;",
        std::vector<std::string>{userId});
    std::string role = queryScalar(
        db_.handle(), "SELECT role FROM users WHERE id=?;",
        std::vector<std::string>{userId});
    UserAccount u;
    u.id = userId;
    u.username = username;
    u.role = role;
    return common::Result<UserAccount>::ok(std::move(u));
}

common::Result<std::vector<UserAccount>> UserService::listUsers() {
    std::vector<UserAccount> out;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_.handle(),
                           "SELECT id, username, role FROM users ORDER BY username;",
                           -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::vector<UserAccount>>::err(
            common::ErrorCode::DatabaseError, "prepare failed");
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        UserAccount u;
        const unsigned char* id = sqlite3_column_text(stmt, 0);
        const unsigned char* un = sqlite3_column_text(stmt, 1);
        const unsigned char* role = sqlite3_column_text(stmt, 2);
        if (id) u.id = reinterpret_cast<const char*>(id);
        if (un) u.username = reinterpret_cast<const char*>(un);
        if (role) u.role = reinterpret_cast<const char*>(role);
        out.push_back(std::move(u));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<UserAccount>>::ok(std::move(out));
}

common::Result<void> UserService::changeRole(const std::string& userId,
                                             const std::string& newRole) {
    if (!isValidRole(newRole)) {
        return common::Result<void>::err(common::ErrorCode::InvalidArgument,
                                         "invalid role '" + newRole + "'");
    }
    std::string existing = queryScalar(
        db_.handle(), "SELECT id FROM users WHERE id=?;",
        std::vector<std::string>{userId});
    if (existing.empty()) {
        return common::Result<void>::err(common::ErrorCode::NotFound,
                                         "user not found: " + userId);
    }
    auto res = exec(db_.handle(),
                    "UPDATE users SET role=?, version=version+1 WHERE id=?;",
                    std::vector<std::string>{newRole, userId});
    if (res.failed()) {
        return common::Result<void>::err(common::ErrorCode::DatabaseError,
                                         res.error());
    }
    return common::Result<void>::ok();
}

common::Result<void> UserService::grantPermission(
    const std::string& userId, const std::string& permission,
    const std::string& entityType) {
    return rbac_.grantPermission(userId, permission, entityType);
}

common::Result<bool> UserService::hasPermission(
    const std::string& userId, const std::string& permission,
    const std::string& entityType) {
    return rbac_.hasPermission(userId, permission, entityType);
}

common::Result<void> UserService::updateEntity(const std::string& type,
                                               const std::string& id,
                                               const std::string& newData,
                                               int expectedVersion) {
    auto etype = entityTypeFromString(type);
    if (!etype) {
        return common::Result<void>::err(common::ErrorCode::InvalidArgument,
                                         "unknown entity type: " + type);
    }
    auto got = tracelink_.getEntity(*etype, id);
    if (got.failed()) {
        return common::Result<void>::err(got.errorCode(), got.error());
    }
    if (!got.value().has_value()) {
        return common::Result<void>::err(common::ErrorCode::NotFound,
                                         "entity not found: " + id);
    }
    Entity e = got.value().value();
    e.text = newData;
    auto res = tracelink_.updateEntityIfVersion(e, expectedVersion);
    if (res.failed()) {
        return common::Result<void>::err(res.errorCode(), res.error());
    }
    return common::Result<void>::ok();
}

}  // namespace lodestar::tracelink
