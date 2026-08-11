// core/persistence/MigrationRunner.cpp
#include "core/persistence/MigrationRunner.h"

#include <algorithm>
#include <cctype>
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

}  // namespace lodestar::persistence
