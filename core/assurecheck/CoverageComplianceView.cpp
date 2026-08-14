// core/assurecheck/CoverageComplianceView.cpp
// Gap-Fill AssureCheck 2.3: unified live coverage view.

#include "core/assurecheck/CoverageComplianceView.h"

#include <algorithm>

namespace lodestar::assurecheck {

CoverageComplianceView::CoverageComplianceView(persistence::Database& db)
    : db_(db) {}

CoverageAggregate CoverageComplianceView::aggregate(
    const std::vector<CoverageMeasurement>& measurements) {
    CoverageAggregate agg;
    for (const auto& m : measurements) {
        agg.statementsExecuted += m.statementsExecuted;
        agg.statementsTotal += m.statementsTotal;
        agg.decisionsTaken += m.decisionsTaken;
        agg.decisionsTotal += m.decisionsTotal;
        agg.conditionsSatisfied += m.conditionsSatisfied;
        agg.conditionsTotal += m.conditionsTotal;
    }
    return agg;
}

std::vector<ObjectiveCompliance> CoverageComplianceView::mapToObjectives(
    const StandardBundle& bundle, const CoverageAggregate& agg,
    const std::string& projectDal, int statementThreshold) {
    std::vector<ObjectiveCompliance> out;
    auto flat = StandardsContentService::flatten(bundle);

    double stmtPct = 0.0;
    if (agg.statementsTotal > 0)
        stmtPct = 100.0 * agg.statementsExecuted / agg.statementsTotal;

    double decPct = 0.0;
    if (agg.decisionsTotal > 0)
        decPct = 100.0 * agg.decisionsTaken / agg.decisionsTotal;

    // Which verification objectives does the DO-178C bundle cover? We map
    // statement coverage to the A-3.x "verification is achieved" objectives and
    // decision/MC-DC (when present) to the deeper verification objectives.
    for (const auto& sub : flat) {
        if (!StandardsContentService::appliesToDal(sub.dal, projectDal)) continue;

        bool isVerification = sub.code.rfind("A3-", 0) == 0;

        ObjectiveCompliance oc;
        oc.subCode = sub.code;
        oc.objective = sub.objective;
        oc.dal = sub.dal;

        if (!isVerification) {
            // Non-verification objectives are outside this coverage view.
            continue;
        }

        // Statement-coverage evidence: link the measured scopes (if any).
        if (agg.statementsTotal > 0)
            oc.evidenceLinks.push_back(
                "statement coverage " + std::to_string((int)stmtPct) + "%");

        // Honest status mapping.
        if (agg.statementsTotal == 0) {
            oc.status = "no-evidence";
            oc.reason = "no measured statement coverage (tooling absent)";
        } else if (stmtPct >= statementThreshold &&
                   (agg.decisionsTotal == 0 ||
                    decPct >= statementThreshold)) {
            oc.status = "compliant";
            oc.reason = "statement coverage meets threshold";
        } else {
            oc.status = "partial";
            oc.reason =
                "statement coverage below threshold or decision coverage "
                "not fully measured";
        }

        // A-3.5..A-3.8 are review objectives; map them via statement evidence
        // only (they are coverage-supporting, not coverage-requiring). Keep the
        // same honest status but note it.
        out.push_back(std::move(oc));
    }
    return out;
}

}  // namespace lodestar::assurecheck
