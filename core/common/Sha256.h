#pragma once
// core/common/Sha256.h
// Minimal self-contained SHA-256 (FIPS 180-4) used for salted password hashing
// in the S2 Phase 1 user model. No external crypto dependency.

#include <string>

namespace lodestar::common {

// Returns the lowercase hex SHA-256 digest of `data`.
std::string sha256Hex(const std::string& data);

}  // namespace lodestar::common
