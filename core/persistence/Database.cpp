// core/persistence/Database.cpp
#include "core/persistence/Database.h"

#include <sqlite3.h>

namespace lodestar::persistence {

Database::~Database() {
    close();
}

common::Result<void> Database::open(const std::string& path) {
    if (db_ != nullptr) {
        return common::Result<void>::err("database already open");
    }
    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string msg = db_ ? sqlite3_errmsg(db_) : "failed to open database";
        if (db_ != nullptr) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return common::Result<void>::err("sqlite3_open failed: " + msg);
    }
    // Enable foreign keys and WAL journaling for robustness.
    sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
    // WP-F (B7): wait up to 5s for a busy lock instead of failing immediately,
    // so concurrent writers (each on its own connection) can serialize cleanly.
    sqlite3_exec(db_, "PRAGMA busy_timeout = 5000;", nullptr, nullptr, nullptr);
    return common::Result<void>::ok();
}

void Database::close() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

common::Result<void> Database::execute(const std::string& sql) {
    if (db_ == nullptr) {
        return common::Result<void>::err("database not open");
    }
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "unknown error";
        sqlite3_free(errmsg);
        return common::Result<void>::err("sqlite3_exec failed: " + msg);
    }
    return common::Result<void>::ok();
}

long long Database::lastInsertRowId() const {
    return db_ ? sqlite3_last_insert_rowid(db_) : 0;
}

common::Result<void> Database::beginImmediate() {
    if (db_ == nullptr) {
        return common::Result<void>::err("database not open");
    }
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "unknown error";
        sqlite3_free(errmsg);
        return common::Result<void>::err("BEGIN IMMEDIATE failed: " + msg);
    }
    return common::Result<void>::ok();
}

common::Result<void> Database::commit() {
    if (db_ == nullptr) {
        return common::Result<void>::err("database not open");
    }
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "unknown error";
        sqlite3_free(errmsg);
        return common::Result<void>::err("COMMIT failed: " + msg);
    }
    return common::Result<void>::ok();
}

common::Result<void> Database::rollback() {
    if (db_ == nullptr) {
        return common::Result<void>::err("database not open");
    }
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    return common::Result<void>::ok();
}

std::string Database::queryScalar(const std::string& sql) {
    if (db_ == nullptr) {
        return "";
    }
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return "";
    }
    std::string result;
    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_count(stmt) > 0) {
        const unsigned char* txt = sqlite3_column_text(stmt, 0);
        if (txt != nullptr) {
            result.assign(reinterpret_cast<const char*>(txt));
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

common::Result<void> Database::backup(const std::string& destPath) {
    if (db_ == nullptr) {
        return common::Result<void>::err(common::ErrorCode::BackupError,
                                         "database not open");
    }
    sqlite3* dest = nullptr;
    int rc = sqlite3_open(destPath.c_str(), &dest);
    if (rc != SQLITE_OK) {
        std::string msg = dest ? sqlite3_errmsg(dest) : "failed to open backup target";
        if (dest != nullptr) sqlite3_close(dest);
        return common::Result<void>::err(common::ErrorCode::BackupError,
                                         "backup open failed: " + msg);
    }
    sqlite3_backup* bk = sqlite3_backup_init(dest, "main", db_, "main");
    if (bk == nullptr) {
        std::string msg = sqlite3_errmsg(dest);
        sqlite3_close(dest);
        return common::Result<void>::err(common::ErrorCode::BackupError,
                                         "backup init failed: " + msg);
    }
    rc = sqlite3_backup_step(bk, -1);
    sqlite3_backup_finish(bk);
    sqlite3_close(dest);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err(common::ErrorCode::BackupError,
                                         "backup step failed (rc=" +
                                             std::to_string(rc) + ")");
    }
    return common::Result<void>::ok();
}

common::Result<void> Database::restore(const std::string& srcPath) {
    if (db_ == nullptr) {
        return common::Result<void>::err(common::ErrorCode::BackupError,
                                         "database not open");
    }
    sqlite3* src = nullptr;
    int rc = sqlite3_open(srcPath.c_str(), &src);
    if (rc != SQLITE_OK) {
        std::string msg = src ? sqlite3_errmsg(src) : "failed to open restore source";
        if (src != nullptr) sqlite3_close(src);
        return common::Result<void>::err(common::ErrorCode::BackupError,
                                         "restore open failed: " + msg);
    }
    sqlite3_backup* bk = sqlite3_backup_init(db_, "main", src, "main");
    if (bk == nullptr) {
        std::string msg = sqlite3_errmsg(db_);
        sqlite3_close(src);
        return common::Result<void>::err(common::ErrorCode::BackupError,
                                         "restore init failed: " + msg);
    }
    rc = sqlite3_backup_step(bk, -1);
    sqlite3_backup_finish(bk);
    sqlite3_close(src);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err(common::ErrorCode::BackupError,
                                         "restore step failed (rc=" +
                                             std::to_string(rc) + ")");
    }
    return common::Result<void>::ok();
}

}  // namespace lodestar::persistence
