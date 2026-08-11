#pragma once
// core/persistence/MigrationRunner.h
// Applies pending SQL migration files in order, tracking the current schema
// version in the schema_version table.

#include <string>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::persistence {

class MigrationRunner {
public:
    explicit MigrationRunner(Database& db);

    // Applies every migration file in migrationsDir whose numeric prefix is
    // greater than the current schema version. Returns the new version.
    common::Result<int> run(const std::string& migrationsDir);

    // Current schema version (0 if the table is absent/empty).
    int currentVersion() const;

private:
    Database& db_;
};

}  // namespace lodestar::persistence
