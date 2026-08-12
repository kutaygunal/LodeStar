// core/assurecheck/EvidenceService.cpp
// Phase 11 WP-3 (AssureCheck): evidence + integration implementation.

#include "core/assurecheck/EvidenceService.h"

namespace lodestar::assurecheck {

using lodestar::testforge::RunStatus;
using lodestar::tracelink::EntityFilter;
using lodestar::tracelink::EntityType;

EvidenceService::EvidenceService(persistence::Database& db,
                                 tracelink::TraceLinkService& tl,
                                 testforge::TestForgeDao& tf)
    : db_(db), tl_(tl), tf_(tf) {}

common::Result<EvidenceSnapshot> EvidenceService::collect() {
    EvidenceSnapshot snap;

    auto reqs = tl_.listEntities(EntityType::Requirement, EntityFilter{});
    if (reqs.failed()) {
        return common::Result<EvidenceSnapshot>::err(reqs.error());
    }
    for (const auto& e : reqs.value()) snap.requirementIds.push_back(e.id);

    auto designs = tl_.listEntities(EntityType::Design, EntityFilter{});
    if (designs.failed()) {
        return common::Result<EvidenceSnapshot>::err(designs.error());
    }
    for (const auto& e : designs.value()) snap.designIds.push_back(e.id);

    auto tcs = tl_.listEntities(EntityType::TestCase, EntityFilter{});
    if (tcs.failed()) {
        return common::Result<EvidenceSnapshot>::err(tcs.error());
    }
    for (const auto& e : tcs.value()) snap.testCaseIds.push_back(e.id);

    auto links = tl_.allLinks();
    if (links.failed()) {
        return common::Result<EvidenceSnapshot>::err(links.error());
    }
    for (const auto& l : links.value()) snap.traceLinkIds.push_back(l.id);

    auto runs = tf_.listRuns();
    if (runs.failed()) {
        return common::Result<EvidenceSnapshot>::err(runs.error());
    }
    for (const auto& run : runs.value()) {
        switch (run.status) {
            case RunStatus::Passed:
                snap.passedRunIds.push_back(run.id);
                break;
            case RunStatus::Failed:
                snap.failedRunIds.push_back(run.id);
                break;
            case RunStatus::Blocked:
                snap.blockedRunIds.push_back(run.id);
                break;
            default:
                break;  // Pending / Running are not classified
        }
    }

    return common::Result<EvidenceSnapshot>::ok(std::move(snap));
}

common::Result<std::vector<CheckResult>> EvidenceService::runChecks(
    const std::string& standardCode, const std::string& dalLevel) {
    auto snap = collect();
    if (snap.failed()) {
        return common::Result<std::vector<CheckResult>>::err(snap.error());
    }
    ComplianceEngine engine(db_);
    return engine.runChecksWithEvidence(standardCode, dalLevel, snap.value());
}

common::Result<void> EvidenceService::storeResults(
    const std::vector<CheckResult>& results) {
    ComplianceEngine engine(db_);
    return engine.storeResults(results);
}

common::Result<std::vector<CheckResult>> EvidenceService::resultsFor(
    const std::string& standardCode) {
    ComplianceEngine engine(db_);
    return engine.resultsFor(standardCode);
}

}  // namespace lodestar::assurecheck
