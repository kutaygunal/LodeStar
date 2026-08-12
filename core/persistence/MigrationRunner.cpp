// core/persistence/MigrationRunner.cpp
#include "core/persistence/MigrationRunner.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include <sqlite3.h>

namespace fs = std::filesystem;

namespace lodestar::persistence {

MigrationRunner::MigrationRunner(Database& db) : db_(db) {}

int MigrationRunner::currentVersion() const {
    if (!db_.isOpen()) {
        return 0;
    }
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_.handle(),
                                "SELECT COALESCE(MAX(version), 0) FROM schema_version;",
                                -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }
    int version = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        version = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return version;
}

namespace {
// Parses the leading numeric prefix of a migration filename, e.g. "001_initial.sql" -> 1.
int migrationNumber(const std::string& filename) {
    int value = 0;
    bool any = false;
    for (char c : filename) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            value = value * 10 + (c - '0');
            any = true;
        } else {
            break;
        }
    }
    return any ? value : -1;
}

// Collects the sorted numeric prefixes of every migration file in migrationsDir.
std::vector<int> collectMigrationNumbers(const std::string& migrationsDir,
                                         std::string* errOut) {
    std::vector<int> nums;
    std::error_code ec;
    if (!fs::exists(migrationsDir, ec)) {
        if (errOut) *errOut = "migrations directory not found: " + migrationsDir;
        return nums;
    }
    for (const auto& entry : fs::directory_iterator(migrationsDir, ec)) {
        if (!entry.is_regular_file()) continue;
        int num = migrationNumber(entry.path().filename().string());
        if (num >= 0) nums.push_back(num);
    }
    std::sort(nums.begin(), nums.end());
    // Multiple files may share a numeric prefix (e.g. two 010_* files from
    // parallel work packages); treat them as one schema version for safety
    // checks so the applied set and the file set compare equal.
    nums.erase(std::unique(nums.begin(), nums.end()), nums.end());
    return nums;
}

// Reads the sorted set of applied migration versions from schema_version.
std::vector<int> appliedVersions(sqlite3* db) {
    std::vector<int> applied;
    if (db == nullptr) return applied;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT version FROM schema_version;", -1, &stmt,
                           nullptr) != SQLITE_OK) {
        return applied;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        applied.push_back(sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);
    std::sort(applied.begin(), applied.end());
    applied.erase(std::unique(applied.begin(), applied.end()), applied.end());
    return applied;
}
}  // namespace

common::Result<int> MigrationRunner::run(const std::string& migrationsDir) {
    if (!db_.isOpen()) {
        return common::Result<int>::err("database not open");
    }

    // Ensure the schema_version table exists.
    auto ensure = db_.execute(
        "CREATE TABLE IF NOT EXISTS schema_version ("
        "  version INTEGER NOT NULL"
        ");");
    if (ensure.failed()) {
        return common::Result<int>::err("failed to create schema_version: " + ensure.error());
    }

    std::error_code ec;
    if (!fs::exists(migrationsDir, ec)) {
        return common::Result<int>::err("migrations directory not found: " + migrationsDir);
    }

    // Collect migration files sorted by numeric prefix.
    std::vector<std::pair<int, fs::path>> migrations;
    for (const auto& entry : fs::directory_iterator(migrationsDir, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string name = entry.path().filename().string();
        int num = migrationNumber(name);
        if (num < 0) {
            continue;
        }
        migrations.emplace_back(num, entry.path());
    }
    std::sort(migrations.begin(), migrations.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    int current = currentVersion();
    int applied = current;

    for (const auto& [num, path] : migrations) {
        if (num <= current) {
            continue;
        }
        std::ifstream in(path);
        if (!in) {
            return common::Result<int>::err("cannot read migration: " + path.string());
        }
        std::stringstream buffer;
        buffer << in.rdbuf();
        std::string sql = buffer.str();

        // Apply inside a transaction.
        auto begin = db_.execute("BEGIN;");
        if (begin.failed()) {
            return common::Result<int>::err("BEGIN failed: " + begin.error());
        }
        auto exec = db_.execute(sql);
        if (exec.failed()) {
            db_.execute("ROLLBACK;");
            return common::Result<int>::err("migration " + path.filename().string() +
                                            " failed: " + exec.error());
        }
        auto bump = db_.execute("INSERT INTO schema_version (version) VALUES (" +
                                std::to_string(num) + ");");
        if (bump.failed()) {
            db_.execute("ROLLBACK;");
            return common::Result<int>::err("version update failed: " + bump.error());
        }
        auto commit = db_.execute("COMMIT;");
        if (commit.failed()) {
            db_.execute("ROLLBACK;");
            return common::Result<int>::err("COMMIT failed: " + commit.error());
        }
        applied = num;
    }

    return common::Result<int>::ok(applied);
}

common::Result<bool> MigrationRunner::dryRun(const std::string& migrationsDir) {
    if (!db_.isOpen()) {
        return common::Result<bool>::err(common::ErrorCode::MigrationError,
                                         "database not open");
    }
    std::string err;
    auto nums = collectMigrationNumbers(migrationsDir, &err);
    if (!err.empty()) {
        return common::Result<bool>::err(common::ErrorCode::MigrationError, err);
    }
    int current = currentVersion();
    for (int n : nums) {
        if (n > current) {
            return common::Result<bool>::ok(true);
        }
    }
    return common::Result<bool>::ok(false);
}

std::string MigrationRunner::checksum() const {
    auto applied = appliedVersions(db_.isOpen() ? db_.handle() : nullptr);
    // FNV-1a 64-bit over the sorted applied version list.
    std::uint64_t h = 1469598103934665603ULL;
    for (int v : applied) {
        std::string s = std::to_string(v) + ",";
        for (char c : s) {
            h ^= static_cast<unsigned char>(c);
            h *= 1099511628211ULL;
        }
    }
    char buf[17] = {0};
    std::snprintf(buf, sizeof(buf), "%016llx",
                  static_cast<unsigned long long>(h));
    return std::string(buf);
}

common::Result<bool> MigrationRunner::verify(const std::string& migrationsDir) {
    if (!db_.isOpen()) {
        return common::Result<bool>::err(common::ErrorCode::MigrationError,
                                         "database not open");
    }
    std::string err;
    auto fileNums = collectMigrationNumbers(migrationsDir, &err);
    if (!err.empty()) {
        return common::Result<bool>::err(common::ErrorCode::MigrationError, err);
    }
    auto applied = appliedVersions(db_.handle());
    return common::Result<bool>::ok(applied == fileNums);
}

}  // namespace lodestar::persistence
