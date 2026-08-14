// core/integratehub/CertificationControlService.h
// Gap-Fill IntegrateHub 6.2: integration with certification control.
//
// Feeds PR/CR + impact into the AssureCheck configuration-control view and
// emits the PR/CR log as a certification artifact. Reuses the shared reporting
// pattern so the log can be surfaced in the certification evidence package.

#pragma once

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/integratehub/ImpactAnalysisService.h"

namespace lodestar::integratehub {

// One certification-control view row: a CR with its linked PR and impact.
struct CertControlRow {
    std::string crId;
    std::string crTitle;
    std::string crStatus;
    std::string prId;
    std::string prTitle;
    std::string prStatus;
    int impactCount = 0;
    int unverifiedImpact = 0;  // risk of unverified impact
};

class CertificationControlService {
public:
    explicit CertificationControlService(persistence::Database& db);

    // The configuration-control view: every CR joined with its PR (if any) and
    // its impact set, ordered for certification review.
    common::Result<std::vector<CertControlRow>> configControlView();

    // Emit the PR/CR log as a certification artifact (structured text).
    common::Result<std::string> emitPrCrLog();

private:
    persistence::Database& db_;
};

}  // namespace lodestar::integratehub
