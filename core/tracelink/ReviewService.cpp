#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// core/tracelink/ReviewService.cpp
// WP-2 (Phase 10): general artifact review / comment / approval.

#include "core/tracelink/ReviewService.h"

#include <ctime>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "core/common/Uuid.h"

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

Comment commentFromRow(const std::vector<std::string>& r) {
    Comment c;
    c.id = r[0];
    c.entityType = r[1];
    c.entityId = r[2];
    c.author = r[3];
    c.body = r[4];
    c.createdAt = r[5];
    return c;
}

Review reviewFromRow(const std::vector<std::string>& r) {
    Review v;
    v.id = r[0];
    v.entityType = r[1];
    v.entityId = r[2];
    v.reviewer = r[3];
    v.verdict = r[4];
    v.comment = r[5];
    v.createdAt = r[6];
    return v;
}

}  // namespace

ReviewService::ReviewService(persistence::Database& db) : db_(db) {}

common::Result<Comment> ReviewService::addComment(const std::string& entityType,
                                                  const std::string& entityId,
                                                  const std::string& author,
                                                  const std::string& body) {
    Comment c;
    c.id = newUuid();
    c.entityType = entityType;
    c.entityId = entityId;
    c.author = author;
    c.body = body;
    c.createdAt = now();

    auto res = exec(db_.handle(),
                    "INSERT INTO comments (id, entity_type, entity_id, author, body, "
                    "created_at) VALUES (?,?,?,?,?,?);",
                    {c.id, c.entityType, c.entityId, c.author, c.body, c.createdAt});
    if (res.failed()) return common::Result<Comment>::err(res.error());
    return common::Result<Comment>::ok(std::move(c));
}

common::Result<std::vector<Comment>> ReviewService::commentsFor(
    const std::string& entityType, const std::string& entityId) {
    auto rows = query(db_.handle(),
                      "SELECT id, entity_type, entity_id, author, body, created_at "
                      "FROM comments WHERE entity_type=? AND entity_id=? "
                      "ORDER BY created_at ASC, rowid ASC;",
                      {entityType, entityId}, 6);
    if (rows.failed()) {
        return common::Result<std::vector<Comment>>::err(rows.error());
    }
    std::vector<Comment> out;
    for (const auto& r : rows.value()) out.push_back(commentFromRow(r));
    return common::Result<std::vector<Comment>>::ok(std::move(out));
}

common::Result<Review> ReviewService::submitReview(const std::string& entityType,
                                                   const std::string& entityId,
                                                   const std::string& reviewer,
                                                   const std::string& verdict,
                                                   const std::string& comment) {
    Review v;
    v.id = newUuid();
    v.entityType = entityType;
    v.entityId = entityId;
    v.reviewer = reviewer;
    v.verdict = verdict;
    v.comment = comment;
    v.createdAt = now();

    auto res = exec(db_.handle(),
                    "INSERT INTO reviews (id, entity_type, entity_id, reviewer, verdict, "
                    "comment, created_at) VALUES (?,?,?,?,?,?,?);",
                    {v.id, v.entityType, v.entityId, v.reviewer, v.verdict, v.comment,
                     v.createdAt});
    if (res.failed()) return common::Result<Review>::err(res.error());
    return common::Result<Review>::ok(std::move(v));
}

common::Result<std::vector<Review>> ReviewService::reviewsFor(
    const std::string& entityType, const std::string& entityId) {
    auto rows = query(db_.handle(),
                      "SELECT id, entity_type, entity_id, reviewer, verdict, comment, "
                      "created_at FROM reviews WHERE entity_type=? AND entity_id=? "
                      "ORDER BY created_at DESC, rowid DESC;",
                      {entityType, entityId}, 7);
    if (rows.failed()) {
        return common::Result<std::vector<Review>>::err(rows.error());
    }
    std::vector<Review> out;
    for (const auto& r : rows.value()) out.push_back(reviewFromRow(r));
    return common::Result<std::vector<Review>>::ok(std::move(out));
}

common::Result<std::string> ReviewService::approvalStatus(
    const std::string& entityType, const std::string& entityId) {
    auto rows = query(db_.handle(),
                      "SELECT verdict FROM reviews WHERE entity_type=? AND entity_id=? "
                      "ORDER BY created_at DESC, rowid DESC LIMIT 1;",
                      {entityType, entityId}, 1);
    if (rows.failed()) {
        return common::Result<std::string>::err(rows.error());
    }
    if (rows.value().empty()) {
        return common::Result<std::string>::ok(std::string("None"));
    }
    const std::string& verdict = rows.value().front().front();
    // Map the stored verdict to the approval status string.
    if (verdict == "Approve") {
        return common::Result<std::string>::ok(std::string("Approved"));
    }
    if (verdict == "Reject") {
        return common::Result<std::string>::ok(std::string("Rejected"));
    }
    if (verdict == "RequestChanges") {
        return common::Result<std::string>::ok(std::string("RequestChanges"));
    }
    return common::Result<std::string>::ok(std::string("None"));
}

}  // namespace lodestar::tracelink
