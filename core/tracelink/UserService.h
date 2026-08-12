#pragma once
// core/tracelink/UserService.h
// S2 Phase 1: user accounts with login, roles, permissions and concurrent-edit
// conflict handling.
//
// Surfaces the WP-4 RBAC tables (users/roles/user_permissions, migration 016)
// as real accounts: salted password hashes, login sessions (migration 023),
// role changes, permission grants, and optimistic-lock entity updates that
// reject stale concurrent writes with a ConcurrencyError.

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/tracelink/RbacService.h"
#include "core/tracelink/TraceLinkService.h"

namespace lodestar::tracelink {

// A user account with a single role (admin|editor|reviewer|viewer).
struct UserAccount {
    std::string id;
    std::string username;
    std::string role;
};

class UserService {
public:
    explicit UserService(persistence::Database& db);

    // Creates a user with a salted password hash (never stored in plaintext).
    // Fails on a duplicate username.
    common::Result<UserAccount> registerUser(const std::string& username,
                                             const std::string& password,
                                             const std::string& role);

    // Returns a session token on success; fails on wrong password / unknown user.
    common::Result<std::string> login(const std::string& username,
                                      const std::string& password);

    // Invalidates the session token.
    common::Result<void> logout(const std::string& token);

    // Returns the user for a valid, unexpired session; fails otherwise.
    common::Result<UserAccount> currentUser(const std::string& token);

    // Lists all users.
    common::Result<std::vector<UserAccount>> listUsers();

    // Updates a user's role (admin|editor|reviewer|viewer).
    common::Result<void> changeRole(const std::string& userId,
                                    const std::string& newRole);

    // Grants a permission to a user (delegates to RbacService).
    common::Result<void> grantPermission(const std::string& userId,
                                         const std::string& permission,
                                         const std::string& entityType = "");

    // True if the user has the permission (admin always has all).
    common::Result<bool> hasPermission(const std::string& userId,
                                       const std::string& permission,
                                       const std::string& entityType = "");

    // Optimistic-lock update of an entity's text. Succeeds only when
    // expectedVersion matches the currently stored version (and bumps it);
    // otherwise returns a ConcurrencyError and does NOT overwrite.
    common::Result<void> updateEntity(const std::string& type,
                                      const std::string& id,
                                      const std::string& newData,
                                      int expectedVersion);

private:
    persistence::Database& db_;
    RbacService rbac_;
    TraceLinkService tracelink_;
};

}  // namespace lodestar::tracelink
