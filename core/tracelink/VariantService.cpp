#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// core/tracelink/VariantService.cpp
// S2 Phase 16 (Variants / branching): product-line engineering.

#include "core/tracelink/VariantService.h"

#include <cstdio>
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

// Runs a SELECT expecting ncols text columns; returns one row per result.
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

int toInt(const std::string& s, int fallback = 0) {
    if (s.empty()) return fallback;
    try {
        return std::stoi(s);
    } catch (...) {
        return fallback;
    }
}

// Returns the current version of a requirement in a variant, or 0 if absent.
int variantVersion(sqlite3* db, const std::string& variantId,
                   const std::string& requirementId) {
    auto rows = query(db,
                      "SELECT version FROM variant_requirements WHERE variant_id=? "
                      "AND requirement_id=?;",
                      {variantId, requirementId}, 1);
    if (rows.failed() || rows.value().empty()) return 0;
    return toInt(rows.value().front()[0]);
}

// Returns the current included state of a requirement in a variant (0 if absent).
int variantIncluded(sqlite3* db, const std::string& variantId,
                    const std::string& requirementId) {
    auto rows = query(db,
                      "SELECT included FROM variant_requirements WHERE variant_id=? "
                      "AND requirement_id=?;",
                      {variantId, requirementId}, 1);
    if (rows.failed() || rows.value().empty()) return 0;
    return toInt(rows.value().front()[0]);
}

// Upserts a requirement membership in a variant, bumping its version.
common::Result<void> setVariantRequirement(sqlite3* db, const std::string& variantId,
                                           const std::string& requirementId,
                                           int included) {
    int v = variantVersion(db, variantId, requirementId) + 1;
    return exec(db,
                "INSERT INTO variant_requirements (variant_id, requirement_id, "
                "included, version, updated_at) VALUES (?,?,?,?,?) "
                "ON CONFLICT(variant_id, requirement_id) DO UPDATE SET "
                "included=excluded.included, version=excluded.version, "
                "updated_at=excluded.updated_at;",
                {variantId, requirementId, std::to_string(included),
                 std::to_string(v), now()});
}

// Returns the current version of a requirement on a branch, or 0 if absent.
int branchVersion(sqlite3* db, const std::string& branchId,
                  const std::string& requirementId) {
    auto rows = query(db,
                      "SELECT version FROM branch_requirements WHERE branch_id=? "
                      "AND requirement_id=?;",
                      {branchId, requirementId}, 1);
    if (rows.failed() || rows.value().empty()) return 0;
    return toInt(rows.value().front()[0]);
}

// Upserts a requirement membership on a branch, bumping its version.
common::Result<void> setBranchRequirement(sqlite3* db, const std::string& branchId,
                                          const std::string& requirementId,
                                          int included) {
    int v = branchVersion(db, branchId, requirementId) + 1;
    return exec(db,
                "INSERT INTO branch_requirements (branch_id, requirement_id, "
                "included, version, base_version, updated_at) VALUES (?,?,?,?,?,?) "
                "ON CONFLICT(branch_id, requirement_id) DO UPDATE SET "
                "included=excluded.included, version=excluded.version, "
                "updated_at=excluded.updated_at;",
                {branchId, requirementId, std::to_string(included), std::to_string(v),
                 std::to_string(v), now()});
}

}  // namespace

// ---------------------------------------------------------------------------
// VariantService.
// ---------------------------------------------------------------------------
VariantService::VariantService(persistence::Database& db) : db_(db) {}

common::Result<Variant> VariantService::createVariant(const std::string& name) {
    if (name.empty()) {
        return common::Result<Variant>::err(common::ErrorCode::InvalidArgument,
                                            "variant name must not be empty");
    }
    Variant v;
    v.id = newUuid();
    v.name = name;
    v.createdAt = now();
    auto res = exec(db_.handle(),
                    "INSERT INTO variants (id, name, created_at) VALUES (?,?,?);",
                    {v.id, v.name, v.createdAt});
    if (res.failed()) return common::Result<Variant>::err(res.error());
    return common::Result<Variant>::ok(std::move(v));
}

common::Result<std::vector<Variant>> VariantService::listVariants() {
    auto rows = query(db_.handle(),
                      "SELECT id, name, created_at FROM variants ORDER BY created_at, "
                      "rowid ASC;",
                      {}, 3);
    if (rows.failed()) return common::Result<std::vector<Variant>>::err(rows.error());
    std::vector<Variant> out;
    for (const auto& r : rows.value()) {
        Variant v;
        v.id = r[0];
        v.name = r[1];
        v.createdAt = r[2];
        out.push_back(std::move(v));
    }
    return common::Result<std::vector<Variant>>::ok(std::move(out));
}

common::Result<std::optional<Variant>> VariantService::getVariant(const std::string& id) {
    auto rows = query(db_.handle(),
                      "SELECT id, name, created_at FROM variants WHERE id=?;", {id}, 3);
    if (rows.failed()) return common::Result<std::optional<Variant>>::err(rows.error());
    if (rows.value().empty()) return common::Result<std::optional<Variant>>::ok(std::nullopt);
    const auto& r = rows.value().front();
    Variant v;
    v.id = r[0];
    v.name = r[1];
    v.createdAt = r[2];
    return common::Result<std::optional<Variant>>::ok(std::move(v));
}

common::Result<void> VariantService::addToVariant(const std::string& variantId,
                                                  const std::string& requirementId) {
    auto v = getVariant(variantId);
    if (v.failed()) return common::Result<void>::err(v.error());
    if (!v.value()) {
        return common::Result<void>::err(common::ErrorCode::NotFound,
                                          "variant not found: " + variantId);
    }
    return setVariantRequirement(db_.handle(), variantId, requirementId, 1);
}

common::Result<void> VariantService::removeFromVariant(const std::string& variantId,
                                                       const std::string& requirementId) {
    auto v = getVariant(variantId);
    if (v.failed()) return common::Result<void>::err(v.error());
    if (!v.value()) {
        return common::Result<void>::err(common::ErrorCode::NotFound,
                                          "variant not found: " + variantId);
    }
    return setVariantRequirement(db_.handle(), variantId, requirementId, 0);
}

common::Result<bool> VariantService::variantContains(const std::string& variantId,
                                                     const std::string& requirementId) {
    auto rows = query(db_.handle(),
                      "SELECT included FROM variant_requirements WHERE variant_id=? "
                      "AND requirement_id=?;",
                      {variantId, requirementId}, 1);
    if (rows.failed()) return common::Result<bool>::err(rows.error());
    if (rows.value().empty()) return common::Result<bool>::ok(false);
    return common::Result<bool>::ok(toInt(rows.value().front()[0]) == 1);
}

common::Result<std::vector<std::string>>
VariantService::variantRequirements(const std::string& variantId) {
    auto rows = query(db_.handle(),
                      "SELECT requirement_id FROM variant_requirements WHERE "
                      "variant_id=? AND included=1 ORDER BY rowid ASC;",
                      {variantId}, 1);
    if (rows.failed()) return common::Result<std::vector<std::string>>::err(rows.error());
    std::vector<std::string> out;
    for (const auto& r : rows.value()) out.push_back(r[0]);
    return common::Result<std::vector<std::string>>::ok(std::move(out));
}

common::Result<VariantBranch> VariantService::createBranch(const std::string& baseVariantId,
                                                           const std::string& name) {
    auto v = getVariant(baseVariantId);
    if (v.failed()) return common::Result<VariantBranch>::err(v.error());
    if (!v.value()) {
        return common::Result<VariantBranch>::err(
            common::ErrorCode::NotFound, "variant not found: " + baseVariantId);
    }
    if (name.empty()) {
        return common::Result<VariantBranch>::err(common::ErrorCode::InvalidArgument,
                                                  "branch name must not be empty");
    }

    VariantBranch b;
    b.id = newUuid();
    b.baseVariantId = baseVariantId;
    b.name = name;
    b.createdAt = now();

    auto begin = db_.execute("BEGIN IMMEDIATE;");
    if (begin.failed()) return common::Result<VariantBranch>::err("BEGIN failed");

    auto ins = exec(db_.handle(),
                    "INSERT INTO variant_branches (id, base_variant_id, name, "
                    "created_at) VALUES (?,?,?,?);",
                    {b.id, b.baseVariantId, b.name, b.createdAt});
    if (ins.failed()) {
        db_.execute("ROLLBACK;");
        return common::Result<VariantBranch>::err(ins.error());
    }

    // Copy the base variant's requirement set into the branch, recording the
    // current version as the merge base (base_version).
    auto rows = query(db_.handle(),
                      "SELECT requirement_id, included, version FROM "
                      "variant_requirements WHERE variant_id=?;",
                      {baseVariantId}, 3);
    if (rows.failed()) {
        db_.execute("ROLLBACK;");
        return common::Result<VariantBranch>::err(rows.error());
    }
    for (const auto& r : rows.value()) {
        auto c = exec(db_.handle(),
                      "INSERT INTO branch_requirements (branch_id, requirement_id, "
                      "included, version, base_version, updated_at) VALUES (?,?,?,?,?,?);",
                      {b.id, r[0], r[1], r[2], r[2], b.createdAt});
        if (c.failed()) {
            db_.execute("ROLLBACK;");
            return common::Result<VariantBranch>::err(c.error());
        }
    }

    auto commit = db_.execute("COMMIT;");
    if (commit.failed()) {
        db_.execute("ROLLBACK;");
        return common::Result<VariantBranch>::err("COMMIT failed");
    }
    return common::Result<VariantBranch>::ok(std::move(b));
}

common::Result<std::vector<VariantBranch>> VariantService::listBranches() {
    auto rows = query(db_.handle(),
                      "SELECT id, base_variant_id, name, created_at FROM "
                      "variant_branches ORDER BY created_at, rowid ASC;",
                      {}, 4);
    if (rows.failed()) return common::Result<std::vector<VariantBranch>>::err(rows.error());
    std::vector<VariantBranch> out;
    for (const auto& r : rows.value()) {
        VariantBranch b;
        b.id = r[0];
        b.baseVariantId = r[1];
        b.name = r[2];
        b.createdAt = r[3];
        out.push_back(std::move(b));
    }
    return common::Result<std::vector<VariantBranch>>::ok(std::move(out));
}

common::Result<void> VariantService::addToBranch(const std::string& branchId,
                                                 const std::string& requirementId) {
    auto rows = query(db_.handle(),
                      "SELECT id FROM variant_branches WHERE id=?;", {branchId}, 1);
    if (rows.failed()) return common::Result<void>::err(rows.error());
    if (rows.value().empty()) {
        return common::Result<void>::err(common::ErrorCode::NotFound,
                                         "branch not found: " + branchId);
    }
    return setBranchRequirement(db_.handle(), branchId, requirementId, 1);
}

common::Result<void> VariantService::removeFromBranch(const std::string& branchId,
                                                      const std::string& requirementId) {
    auto rows = query(db_.handle(),
                      "SELECT id FROM variant_branches WHERE id=?;", {branchId}, 1);
    if (rows.failed()) return common::Result<void>::err(rows.error());
    if (rows.value().empty()) {
        return common::Result<void>::err(common::ErrorCode::NotFound,
                                         "branch not found: " + branchId);
    }
    return setBranchRequirement(db_.handle(), branchId, requirementId, 0);
}

common::Result<MergeResult> VariantService::mergeBranch(const std::string& branchId,
                                                        const std::string& targetVariantId) {
    auto tgt = getVariant(targetVariantId);
    if (tgt.failed()) return common::Result<MergeResult>::err(tgt.error());
    if (!tgt.value()) {
        return common::Result<MergeResult>::err(common::ErrorCode::NotFound,
                                                "target variant not found: " + targetVariantId);
    }
    auto br = query(db_.handle(),
                    "SELECT id FROM variant_branches WHERE id=?;", {branchId}, 1);
    if (br.failed()) return common::Result<MergeResult>::err(br.error());
    if (br.value().empty()) {
        return common::Result<MergeResult>::err(common::ErrorCode::NotFound,
                                                "branch not found: " + branchId);
    }

    // Load the branch's requirement set.
    auto rows = query(db_.handle(),
                      "SELECT requirement_id, included, version, base_version FROM "
                      "branch_requirements WHERE branch_id=?;",
                      {branchId}, 4);
    if (rows.failed()) return common::Result<MergeResult>::err(rows.error());

    MergeResult result;
    auto begin = db_.execute("BEGIN IMMEDIATE;");
    if (begin.failed()) return common::Result<MergeResult>::err("BEGIN failed");

    for (const auto& r : rows.value()) {
        const std::string& reqId = r[0];
        int branchState = toInt(r[1]);
        int branchVer = toInt(r[2]);
        int baseVer = toInt(r[3]);

        int targetVer = variantVersion(db_.handle(), targetVariantId, reqId);
        int targetState = variantIncluded(db_.handle(), targetVariantId, reqId);

        bool branchChanged = branchVer != baseVer;
        bool targetChanged = targetVer != baseVer;

        // Conflict: the same requirement was changed differently on both sides
        // since the branch was created. Do NOT overwrite the target.
        if (branchChanged && targetChanged && branchState != targetState) {
            result.status = MergeStatus::Conflict;
            result.conflicts.push_back(reqId);
            continue;
        }

        // Otherwise apply the branch's state to the target (bump its version).
        auto res = setVariantRequirement(db_.handle(), targetVariantId, reqId,
                                         branchState);
        if (res.failed()) {
            db_.execute("ROLLBACK;");
            return common::Result<MergeResult>::err(res.error());
        }
    }

    auto commit = db_.execute("COMMIT;");
    if (commit.failed()) {
        db_.execute("ROLLBACK;");
        return common::Result<MergeResult>::err("COMMIT failed");
    }
    return common::Result<MergeResult>::ok(std::move(result));
}

// --- Variant attribute inheritance / override (3.3) -------------------------

common::Result<void> VariantService::setAttributeOverride(
    const std::string& variantId, const std::string& requirementId,
    const std::string& attribute, const std::string& value) {
    if (variantId.empty() || requirementId.empty() || attribute.empty()) {
        return common::Result<void>::err(common::ErrorCode::InvalidArgument,
                                         "variantId/requirementId/attribute "
                                         "must not be empty");
    }
    auto res = exec(db_.handle(),
                    "INSERT INTO variant_attribute_override "
                    "(variant_id, requirement_id, attribute, value, version, "
                    " updated_at) VALUES (?,?,?,?,?,?) "
                    "ON CONFLICT(variant_id, requirement_id, attribute) "
                    "DO UPDATE SET value=excluded.value, version=version+1, "
                    " updated_at=excluded.updated_at;",
                    {variantId, requirementId, attribute, value, "0", ""});
    if (res.failed()) {
        return common::Result<void>::err(res.error());
    }
    return common::Result<void>::ok();
}

common::Result<void> VariantService::clearAttributeOverride(
    const std::string& variantId, const std::string& requirementId,
    const std::string& attribute) {
    if (variantId.empty() || requirementId.empty() || attribute.empty()) {
        return common::Result<void>::err(common::ErrorCode::InvalidArgument,
                                         "variantId/requirementId/attribute "
                                         "must not be empty");
    }
    auto res = exec(db_.handle(),
                    "DELETE FROM variant_attribute_override WHERE variant_id=? "
                    "AND requirement_id=? AND attribute=?;",
                    {variantId, requirementId, attribute});
    if (res.failed()) {
        return common::Result<void>::err(res.error());
    }
    return common::Result<void>::ok();
}

common::Result<std::string> VariantService::effectiveAttribute(
    const std::string& variantId, const std::string& requirementId,
    const std::string& attribute, const std::string& baseValue) {
    if (variantId.empty() || requirementId.empty() || attribute.empty()) {
        return common::Result<std::string>::err(
            common::ErrorCode::InvalidArgument,
            "variantId/requirementId/attribute must not be empty");
    }
    auto rows = query(db_.handle(),
                      "SELECT value FROM variant_attribute_override WHERE "
                      "variant_id=? AND requirement_id=? AND attribute=?;",
                      {variantId, requirementId, attribute}, 1);
    if (rows.failed()) {
        return common::Result<std::string>::err(rows.error());
    }
    if (rows.value().empty()) {
        // No override -> inherit the base value.
        return common::Result<std::string>::ok(baseValue);
    }
    return common::Result<std::string>::ok(rows.value()[0][0]);
}

}  // namespace lodestar::tracelink
