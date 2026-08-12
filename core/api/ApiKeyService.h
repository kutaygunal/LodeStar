#pragma once
// core/api/ApiKeyService.h
// WP-E (A8): REST API authentication / API keys.
//
// Issues, validates and revokes opaque API keys used to authenticate requests
// to the /tracelink REST surface. Keys are stored in the `api_keys` table
// (migration 012). A key is valid only while it is present AND enabled; a
// revoked key (enabled = 0) is rejected by isValid().

#include <string>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::api {

class ApiKeyService {
public:
    explicit ApiKeyService(persistence::Database& db);

    // Generates a new opaque API key, persists it (enabled) and returns the
    // key value. The caller is responsible for storing the returned value; it
    // is not recoverable from the database afterwards.
    common::Result<std::string> createKey(const std::string& name);

    // Revokes a key (sets enabled = 0). Idempotent: revoking an unknown or
    // already-revoked key is not an error.
    common::Result<void> revokeKey(const std::string& key);

    // True when `key` is present in the table and enabled.
    bool isValid(const std::string& key) const;

private:
    persistence::Database& db_;
};

}  // namespace lodestar::api
