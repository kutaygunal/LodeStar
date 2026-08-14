// core/tracelink/SignatureService.cpp
// Gap-Fill TraceLink 3.4: electronic signatures.

#include "core/tracelink/SignatureService.h"

#include <sstream>

#include <sqlite3.h>

#include "core/common/Sha256.h"
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

bool hasApprovedReview(sqlite3* db, const std::string& entityType,
                       const std::string& entityId) {
    sqlite3_stmt* stmt = nullptr;
    const std::string sql =
        "SELECT COUNT(*) FROM reviews WHERE entity_type=? AND entity_id=? "
        "AND verdict='Approve';";
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    bindText(stmt, 1, entityType);
    bindText(stmt, 2, entityId);
    int n = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) n = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return n > 0;
}

}  // namespace

std::string SignatureService::hashContent(const std::string& content) {
    return lodestar::common::sha256Hex(content);
}

SignatureService::SignatureService(persistence::Database& db) : db_(db) {}

common::Result<Signature> SignatureService::sign(
    const std::string& entityType, const std::string& entityId,
    const std::string& signer, const std::string& role,
    const std::string& content) {
    if (entityId.empty() || signer.empty()) {
        return common::Result<Signature>::err(
            common::ErrorCode::InvalidArgument,
            "entityId and signer must not be empty");
    }
    if (!hasApprovedReview(db_.handle(), entityType, entityId)) {
        return common::Result<Signature>::err(
            common::ErrorCode::ValidationFailed,
            "cannot sign: no approved review on record");
    }
    Signature s;
    s.id = newUuid();
    s.entityType = entityType;
    s.entityId = entityId;
    s.signer = signer;
    s.role = role;
    s.signedAt = nowIso();
    s.contentHash = hashContent(content);

    auto res = exec(db_.handle(),
                    "INSERT INTO esignature (id, entity_type, entity_id, "
                    " signer, role, signed_at, content_hash, review_id) "
                    "VALUES (?,?,?,?,?,?,?,?);",
                    {s.id, s.entityType, s.entityId, s.signer, s.role,
                     s.signedAt, s.contentHash, ""});
    if (res.failed()) {
        return common::Result<Signature>::err(res.error());
    }
    return common::Result<Signature>::ok(std::move(s));
}

common::Result<std::optional<Signature>> SignatureService::lastSignature(
    const std::string& entityType, const std::string& entityId) {
    sqlite3_stmt* stmt = nullptr;
    const std::string sql =
        "SELECT id, entity_type, entity_id, signer, role, signed_at, "
        " content_hash, review_id FROM esignature "
        "WHERE entity_type=? AND entity_id=? "
        "ORDER BY signed_at DESC, rowid DESC LIMIT 1;";
    if (sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        return common::Result<std::optional<Signature>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, entityType);
    bindText(stmt, 2, entityId);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return common::Result<std::optional<Signature>>::ok(std::nullopt);
    }
    Signature s;
    s.id = colText(stmt, 0);
    s.entityType = colText(stmt, 1);
    s.entityId = colText(stmt, 2);
    s.signer = colText(stmt, 3);
    s.role = colText(stmt, 4);
    s.signedAt = colText(stmt, 5);
    s.contentHash = colText(stmt, 6);
    s.reviewId = colText(stmt, 7);
    sqlite3_finalize(stmt);
    return common::Result<std::optional<Signature>>::ok(std::move(s));
}

common::Result<std::vector<Signature>> SignatureService::signaturesFor(
    const std::string& entityType, const std::string& entityId) {
    std::vector<Signature> out;
    sqlite3_stmt* stmt = nullptr;
    const std::string sql =
        "SELECT id, entity_type, entity_id, signer, role, signed_at, "
        " content_hash, review_id FROM esignature "
        "WHERE entity_type=? AND entity_id=? ORDER BY signed_at, rowid;";
    if (sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        return common::Result<std::vector<Signature>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, entityType);
    bindText(stmt, 2, entityId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Signature s;
        s.id = colText(stmt, 0);
        s.entityType = colText(stmt, 1);
        s.entityId = colText(stmt, 2);
        s.signer = colText(stmt, 3);
        s.role = colText(stmt, 4);
        s.signedAt = colText(stmt, 5);
        s.contentHash = colText(stmt, 6);
        s.reviewId = colText(stmt, 7);
        out.push_back(std::move(s));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<Signature>>::ok(std::move(out));
}

common::Result<bool> SignatureService::isValid(
    const std::string& entityType, const std::string& entityId,
    const std::string& currentContent) {
    auto last = lastSignature(entityType, entityId);
    if (last.failed()) {
        return common::Result<bool>::err(last.error());
    }
    if (!last.value().has_value()) {
        return common::Result<bool>::ok(false);
    }
    return common::Result<bool>::ok(
        last.value()->contentHash == hashContent(currentContent));
}

}  // namespace lodestar::tracelink
