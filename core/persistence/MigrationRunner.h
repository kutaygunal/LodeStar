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

    // WP-F (B5): reports whether there are pending (not-yet-applied) migration
    // files in migrationsDir. Returns true if pending, false if up to date.
    common::Result<bool> dryRun(const std::string& migrationsDir);

    // WP-F (B5): a stable checksum of the applied migration set (non-empty).
    // Two databases that applied the same migrations produce the same value.
    std::string checksum() const;

    // WP-F (B5): verifies the applied schema matches the migration files on
    // disk. Returns true if the applied version set equals the file set.
    common::Result<bool> verify(const std::string& migrationsDir);

private:
    Database& db_;
};

}  // namespace lodestar::persistence
