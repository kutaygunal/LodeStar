// core/tracelink/SignatureService.h
// Gap-Fill TraceLink 3.4: electronic signatures.
//
// An approval-signature model (signer, role, timestamp, hash of approved
// content) on top of the existing review/approval workflow. Signatures are
// persisted immutably (no update/delete) and surfaced in certification exports
// (AssureCheck). A signature's validity is checked by re-hashing the current
// content: if it no longer matches the signed hash, the approval is void.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::tracelink {

// One electronic signature.
struct Signature {
    std::string id;
    std::string entityType;
    std::string entityId;
    std::string signer;
    std::string role;
    std::string signedAt;
    std::string contentHash;   // SHA-256 of the approved content
    std::string reviewId;      // the approving review this signs
};

class SignatureService {
public:
    explicit SignatureService(persistence::Database& db);

    // Records an immutable signature for an approved artifact. `content` is the
    // exact approved content; it is hashed and stored. Fails if the artifact
    // has no approved review on record (approval must precede signing).
    common::Result<Signature> sign(const std::string& entityType,
                                   const std::string& entityId,
                                   const std::string& signer,
                                   const std::string& role,
                                   const std::string& content);

    // The most recent signature for an artifact, if any.
    common::Result<std::optional<Signature>> lastSignature(
        const std::string& entityType, const std::string& entityId);

    // All signatures for an artifact, oldest first (for certification export).
    common::Result<std::vector<Signature>> signaturesFor(
        const std::string& entityType, const std::string& entityId);

    // Signature validity on content change: re-hash `content` and compare to the
    // stored hash. Returns false if the content changed since signing.
    common::Result<bool> isValid(const std::string& entityType,
                                 const std::string& entityId,
                                 const std::string& currentContent);

    // SHA-256 hex digest of a string (exposed for tests / deterministic use).
    static std::string hashContent(const std::string& content);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::tracelink
