#pragma once
// core/tracelink/VariantService.h
// S2 Phase 16 (Variants / branching): product-line engineering.
//
// A product variant (e.g. "Base", "Pro", "Avionics") is a named set of
// included/excluded requirements. A branch is a working copy of a variant's
// requirement set that can be modified independently and merged back, with
// conflict detection when the same requirement was changed differently on the
// branch and the target since the branch was created.
//
// Contract written by the scrum-master in docs/s2-phase16-test.md.

#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::tracelink {

// A product variant.
struct Variant {
    std::string id;
    std::string name;
    std::string createdAt;
};

// A branch of a variant.
struct VariantBranch {
    std::string id;
    std::string baseVariantId;
    std::string name;
    std::string createdAt;
};

// Outcome of a merge.
enum class MergeStatus { Merged, Conflict };

struct MergeResult {
    MergeStatus status = MergeStatus::Merged;
    // Requirement ids that conflicted (changed differently on both sides).
    std::vector<std::string> conflicts;
};

class VariantService {
public:
    explicit VariantService(persistence::Database& db);

    // --- Variant model -----------------------------------------------------
    common::Result<Variant> createVariant(const std::string& name);
    common::Result<std::vector<Variant>> listVariants();
    common::Result<std::optional<Variant>> getVariant(const std::string& id);

    // Manage which requirements belong to a variant.
    common::Result<void> addToVariant(const std::string& variantId,
                                      const std::string& requirementId);
    common::Result<void> removeFromVariant(const std::string& variantId,
                                           const std::string& requirementId);
    common::Result<bool> variantContains(const std::string& variantId,
                                         const std::string& requirementId);
    common::Result<std::vector<std::string>>
        variantRequirements(const std::string& variantId);

    // --- Branching ---------------------------------------------------------
    // Creates a branch of a variant (a working copy of its requirement set).
    common::Result<VariantBranch> createBranch(const std::string& baseVariantId,
                                               const std::string& name);
    common::Result<std::vector<VariantBranch>> listBranches();

    // Modify a branch's requirement set (independent of the base variant).
    common::Result<void> addToBranch(const std::string& branchId,
                                     const std::string& requirementId);
    common::Result<void> removeFromBranch(const std::string& branchId,
                                          const std::string& requirementId);

    // Merges branch changes back into the target variant. Detects a conflict
    // when the same requirement was changed differently on the branch and the
    // target since the branch was created; conflicting requirements are NOT
    // overwritten (the merge returns Conflict instead of silently applying).
    common::Result<MergeResult> mergeBranch(const std::string& branchId,
                                            const std::string& targetVariantId);

    // --- Variant attribute inheritance / override (3.3) ---------------------
    // Sets an attribute override for a requirement on a variant. Without an
    // override, a variant inherits the base value (the effective value).
    common::Result<void> setAttributeOverride(const std::string& variantId,
                                              const std::string& requirementId,
                                              const std::string& attribute,
                                              const std::string& value);
    // Clears an override so the variant inherits the base value again.
    common::Result<void> clearAttributeOverride(const std::string& variantId,
                                                const std::string& requirementId,
                                                const std::string& attribute);
    // The effective attribute value for a requirement on a variant: the
    // override if set, otherwise the supplied base value.
    common::Result<std::string> effectiveAttribute(
        const std::string& variantId, const std::string& requirementId,
        const std::string& attribute, const std::string& baseValue);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::tracelink
