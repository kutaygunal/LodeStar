#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// core/tracelink/ChangeRequestService.cpp
// WP-B (A4): change-request + review workflow.

#include "core/tracelink/ChangeRequestService.h"

#include <ctime>
#include <map>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "core/common/Uuid.h"
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

// Tiny flat-JSON parser: {"name":"After","status":"Draft"} -> map. All values
// are treated as strings (the proposed-change payload is a flat field map).
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
                else v += e;
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

ChangeRequest crFromRow(const std::vector<std::string>& r) {
    ChangeRequest c;
    c.id = r[0];
    c.title = r[1];
    c.description = r[2];
    c.status = r[3];
    c.entityType = r[4];
    c.entityId = r[5];
    c.proposedChange = r[6];
    c.createdBy = r[7];
    c.createdAt = r[8];
    c.reviewedBy = r[9];
    c.reviewedAt = r[10];
    c.reviewComment = r[11];
    return c;
}

// Applies one proposed field change to an entity. Unknown fields are ignored.
void applyField(Entity& e, const std::string& field, const std::string& value) {
    if (field == "name") e.name = value;
    else if (field == "text") e.text = value;
    else if (field == "status") e.status = value;
    else if (field == "externalId") e.externalId = value;
    else if (field == "typeAttr") e.typeAttr = value;
    else if (field == "priority") e.priority = value;
    else if (field == "source") e.source = value;
    else if (field == "owner") e.owner = value;
    else if (field == "rationale") e.rationale = value;
    else if (field == "verificationMethod") e.verificationMethod = value;
    else if (field == "safetyLevel") e.safetyLevel = value;
    else if (field == "direction") e.direction = value;
    else if (field == "sourceEntity") e.sourceEntity = value;
    else if (field == "targetEntity") e.targetEntity = value;
    else if (field == "dataItems") e.dataItems = value;
    else if (field == "protocol") e.protocol = value;
    else if (field == "resultStatus") e.resultStatus = value;
    else if (field == "severity") e.severity = value;
    else if (field == "likelihood") e.likelihood = value;
    else if (field == "date") e.date = value;
    else if (field == "parentId") e.parentId = value;
    else if (field == "tags") e.tags = value;
}

}  // namespace

ChangeRequestService::ChangeRequestService(persistence::Database& db) : db_(db) {}

common::Result<ChangeRequest> ChangeRequestService::create(const ChangeRequest& cr) {
    ChangeRequest c = cr;
    if (c.id.empty()) c.id = newUuid();
    if (c.status.empty()) c.status = "Open";
    if (c.createdAt.empty()) c.createdAt = now();
    if (c.proposedChange.empty()) c.proposedChange = "{}";

    auto res = exec(db_.handle(),
                    "INSERT INTO change_requests (id, title, description, status, "
                    "entity_type, entity_id, proposed_change, created_by, created_at, "
                    "reviewed_by, reviewed_at, review_comment) "
                    "VALUES (?,?,?,?,?,?,?,?,?,?,?,?);",
                    {c.id, c.title, c.description, c.status, c.entityType, c.entityId,
                     c.proposedChange, c.createdBy, c.createdAt, c.reviewedBy,
                     c.reviewedAt, c.reviewComment});
    if (res.failed()) return common::Result<ChangeRequest>::err(res.error());
    return common::Result<ChangeRequest>::ok(std::move(c));
}

common::Result<std::vector<ChangeRequest>> ChangeRequestService::reviewQueue() {
    auto rows = query(db_.handle(),
                      "SELECT id, title, description, status, entity_type, entity_id, "
                      "proposed_change, created_by, created_at, reviewed_by, reviewed_at, "
                      "review_comment FROM change_requests "
                      "WHERE status IN ('Open','InReview') "
                      "ORDER BY created_at DESC, rowid DESC;",
                      {}, 12);
    if (rows.failed()) {
        return common::Result<std::vector<ChangeRequest>>::err(rows.error());
    }
    std::vector<ChangeRequest> out;
    for (const auto& r : rows.value()) out.push_back(crFromRow(r));
    return common::Result<std::vector<ChangeRequest>>::ok(std::move(out));
}

common::Result<ChangeRequest> ChangeRequestService::submitForReview(const std::string& id) {
    auto res = exec(db_.handle(),
                    "UPDATE change_requests SET status='InReview' "
                    "WHERE id=? AND status='Open';",
                    {id});
    if (res.failed()) return common::Result<ChangeRequest>::err(res.error());
    auto rows = query(db_.handle(),
                      "SELECT id, title, description, status, entity_type, entity_id, "
                      "proposed_change, created_by, created_at, reviewed_by, reviewed_at, "
                      "review_comment FROM change_requests WHERE id=?;",
                      {id}, 12);
    if (rows.failed()) return common::Result<ChangeRequest>::err(rows.error());
    if (rows.value().empty()) {
        return common::Result<ChangeRequest>::err(common::ErrorCode::NotFound,
                                                    "change request not found: " + id);
    }
    ChangeRequest c = crFromRow(rows.value().front());
    if (c.status != "InReview") {
        return common::Result<ChangeRequest>::err(
            "change request is not in Open status (cannot submit for review)");
    }
    return common::Result<ChangeRequest>::ok(std::move(c));
}

common::Result<ChangeRequest> ChangeRequestService::approve(const std::string& id,
                                                            const std::string& reviewer,
                                                            const std::string& comment) {
    auto res = exec(db_.handle(),
                    "UPDATE change_requests SET status='Approved', reviewed_by=?, "
                    "reviewed_at=?, review_comment=? WHERE id=? AND status='InReview';",
                    {reviewer, now(), comment, id});
    if (res.failed()) return common::Result<ChangeRequest>::err(res.error());
    auto rows = query(db_.handle(),
                      "SELECT id, title, description, status, entity_type, entity_id, "
                      "proposed_change, created_by, created_at, reviewed_by, reviewed_at, "
                      "review_comment FROM change_requests WHERE id=?;",
                      {id}, 12);
    if (rows.failed()) return common::Result<ChangeRequest>::err(rows.error());
    if (rows.value().empty()) {
        return common::Result<ChangeRequest>::err(common::ErrorCode::NotFound,
                                                    "change request not found: " + id);
    }
    ChangeRequest c = crFromRow(rows.value().front());
    if (c.status != "Approved") {
        return common::Result<ChangeRequest>::err(
            "change request is not in InReview status (cannot approve)");
    }
    return common::Result<ChangeRequest>::ok(std::move(c));
}

common::Result<ChangeRequest> ChangeRequestService::reject(const std::string& id,
                                                           const std::string& reviewer,
                                                           const std::string& comment) {
    auto res = exec(db_.handle(),
                    "UPDATE change_requests SET status='Rejected', reviewed_by=?, "
                    "reviewed_at=?, review_comment=? WHERE id=? AND status='InReview';",
                    {reviewer, now(), comment, id});
    if (res.failed()) return common::Result<ChangeRequest>::err(res.error());
    auto rows = query(db_.handle(),
                      "SELECT id, title, description, status, entity_type, entity_id, "
                      "proposed_change, created_by, created_at, reviewed_by, reviewed_at, "
                      "review_comment FROM change_requests WHERE id=?;",
                      {id}, 12);
    if (rows.failed()) return common::Result<ChangeRequest>::err(rows.error());
    if (rows.value().empty()) {
        return common::Result<ChangeRequest>::err(common::ErrorCode::NotFound,
                                                    "change request not found: " + id);
    }
    ChangeRequest c = crFromRow(rows.value().front());
    if (c.status != "Rejected") {
        return common::Result<ChangeRequest>::err(
            "change request is not in InReview status (cannot reject)");
    }
    return common::Result<ChangeRequest>::ok(std::move(c));
}

common::Result<Entity> ChangeRequestService::applyChangeRequest(const std::string& crId) {
    auto rows = query(db_.handle(),
                      "SELECT id, title, description, status, entity_type, entity_id, "
                      "proposed_change, created_by, created_at, reviewed_by, reviewed_at, "
                      "review_comment FROM change_requests WHERE id=?;",
                      {crId}, 12);
    if (rows.failed()) return common::Result<Entity>::err(rows.error());
    if (rows.value().empty()) {
        return common::Result<Entity>::err(common::ErrorCode::NotFound,
                                            "change request not found: " + crId);
    }
    ChangeRequest c = crFromRow(rows.value().front());
    if (c.status != "Approved") {
        return common::Result<Entity>::err(
            "change request is not Approved (cannot apply)");
    }

    auto typeOpt = entityTypeFromString(c.entityType);
    if (!typeOpt) {
        return common::Result<Entity>::err(common::ErrorCode::InvalidArgument,
                                           "invalid entity type: " + c.entityType);
    }
    EntityType type = *typeOpt;

    TraceLinkService svc(db_);
    auto existing = svc.getEntity(type, c.entityId);
    if (existing.failed()) return common::Result<Entity>::err(existing.error());
    if (!existing.value()) {
        return common::Result<Entity>::err(common::ErrorCode::NotFound,
                                            "target entity not found: " + c.entityId);
    }

    // Apply the proposed field changes to the current entity.
    Entity updated = *existing.value();
    auto changes = parseFlatJson(c.proposedChange);
    for (const auto& [field, value] : changes) {
        applyField(updated, field, value);
    }

    // Stamp every audit row written by this change with the CR id.
    svc.setAuditContext(c.createdBy, crId);
    auto applied = svc.updateEntity(updated);
    svc.setAuditContext("", "");
    if (applied.failed()) return common::Result<Entity>::err(applied.error());

    // Mark the CR Implemented.
    auto mark = exec(db_.handle(),
                     "UPDATE change_requests SET status='Implemented' WHERE id=?;",
                     {crId});
    if (mark.failed()) return common::Result<Entity>::err(mark.error());

    return common::Result<Entity>::ok(applied.value());
}

}  // namespace lodestar::tracelink
