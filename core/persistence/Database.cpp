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

}  // namespace lodestar::persistence
