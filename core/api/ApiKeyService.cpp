#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// core/api/ApiKeyService.cpp
// WP-E (A8): REST API authentication / API keys.

#include "core/api/ApiKeyService.h"

#include <ctime>
#include <string>

#include <sqlite3.h>

#include "core/common/Uuid.h"

namespace lodestar::api {

using lodestar::common::newUuid;

namespace {

std::string now() {
    char buf[32];
    const auto t = std::time(nullptr);
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return buf;
}

// Builds an opaque, high-entropy API key from two UUIDs (36 chars each) with
// the hyphens stripped, giving a 64-char alphanumeric token.
std::string generateKey() {
    std::string a = newUuid();
    std::string b = newUuid();
    std::string out;
    out.reserve(64);
    for (char c : a) if (c != '-') out.push_back(c);
    for (char c : b) if (c != '-') out.push_back(c);
    return out;
}

}  // namespace

ApiKeyService::ApiKeyService(persistence::Database& db) : db_(db) {}

common::Result<std::string> ApiKeyService::createKey(const std::string& name) {
    if (!db_.isOpen()) {
        return common::Result<std::string>::err("database not open");
    }
    const std::string key = generateKey();
    const std::string id = newUuid();
    const std::string ts = now();

    sqlite3* db = db_.handle();
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO api_keys (id, key, name, enabled, created_at) "
        "VALUES (?,?,?,1,?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::string>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db)));
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), static_cast<int>(id.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, key.c_str(), static_cast<int>(key.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, name.c_str(), static_cast<int>(name.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, ts.c_str(), static_cast<int>(ts.size()), SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    std::string msg = sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<std::string>::err("step failed: " + msg);
    }
    return common::Result<std::string>::ok(key);
}

common::Result<void> ApiKeyService::revokeKey(const std::string& key) {
    if (!db_.isOpen()) {
        return common::Result<void>::err("database not open");
    }
    sqlite3* db = db_.handle();
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE api_keys SET enabled = 0 WHERE key = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<void>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db)));
    }
    sqlite3_bind_text(stmt, 1, key.c_str(), static_cast<int>(key.size()), SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    std::string msg = sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err("step failed: " + msg);
    }
    return common::Result<void>::ok();
}

bool ApiKeyService::isValid(const std::string& key) const {
    if (!db_.isOpen() || key.empty()) return false;
    sqlite3* db = db_.handle();
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT enabled FROM api_keys WHERE key = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, key.c_str(), static_cast<int>(key.size()), SQLITE_TRANSIENT);
    bool valid = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        valid = sqlite3_column_int(stmt, 0) == 1;
    }
    sqlite3_finalize(stmt);
    return valid;
}

}  // namespace lodestar::api
