#pragma once
// core/tracelink/RbacService.h
// WP-4: User roles + permissions (RBAC) on entities/links.
//
// Enforces role-based access control backed by the `users`, `roles` and
// `user_permissions` tables (migration 016). A user has a single role
// (admin|editor|reviewer|viewer); fine-grained permissions are granted per
// user, optionally scoped to one entity type ('' = all types). The `admin`
// role implicitly holds every permission.

#include <string>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::tracelink {

// A user account with a single role.
struct User {
    std::string id;
    std::string username;
    std::string role;   // admin|editor|reviewer|viewer
};

class RbacService {
public:
    explicit RbacService(persistence::Database& db);

    // Creates a user (assigns UUID if id empty). Fails on duplicate username.
    common::Result<User> createUser(const std::string& username,
                                     const std::string& role);

    // Grants a permission to a user (optionally scoped to one entity type).
    common::Result<void> grantPermission(const std::string& userId,
                                         const std::string& permission,
                                         const std::string& entityType = "");

    // True if the user has the permission (admin always has all permissions).
    common::Result<bool> hasPermission(const std::string& userId,
                                       const std::string& permission,
                                       const std::string& entityType = "");

    // Enforces a permission; fails with an error if the user lacks it.
    common::Result<void> requirePermission(const std::string& userId,
                                           const std::string& permission,
                                           const std::string& entityType = "");

private:
    persistence::Database& db_;
};

}  // namespace lodestar::tracelink
