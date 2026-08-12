#pragma once
// core/persistence/Database.h
// RAII wrapper around a SQLite connection. Owns the sqlite3* handle and
// provides simple statement execution helpers.

#include <string>

#include "core/common/Result.h"

struct sqlite3;

namespace lodestar::persistence {

class Database {
public:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Opens (creating if needed) the SQLite database at the given path.
    common::Result<void> open(const std::string& path);
    void close();
    bool isOpen() const { return db_ != nullptr; }

    sqlite3* handle() { return db_; }

    // Executes a single SQL statement (no parameters).
    common::Result<void> execute(const std::string& sql);

    // BEGIN IMMEDIATE: acquires a write lock immediately so a multi-row
    // mutation (entity+audit, add link, baseline snapshot, import batch) is
    // atomic and cannot deadlock on a later write. Use with commit()/rollback().
    common::Result<void> beginImmediate();
    common::Result<void> commit();
    common::Result<void> rollback();

    // Returns the first column of the first row of `sql`, or "" if none.
    std::string queryScalar(const std::string& sql);

    // Row id of the most recent successful INSERT.
    long long lastInsertRowId() const;

private:
    sqlite3* db_ = nullptr;
};

}  // namespace lodestar::persistence
