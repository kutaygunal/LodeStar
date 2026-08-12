#pragma once
// core/assurecheck/CertReportService.h
// S2 Phase 8: certification-ready reporting + traceability.
//
// Produces PDF / Word (docx) / ReQIF exports of compliance reports and of
// requirements + trace links, and resolves result->requirement traceability
// (which requirement(s) a test result verifies via trace links).
//
// Contract written by the scrum-master in docs/s2-phase8-test.md and
// core/test/s2_phase8_tests.cpp.

#include <cstdint>
#include <string>
#include <vector>

#include "core/assurecheck/ReportService.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/tracelink/Types.h"

namespace lodestar::assurecheck {

// Certification-ready export + traceability service (S2 Phase 8).
class CertReportService {
public:
    explicit CertReportService(persistence::Database& db);

    // (A) PDF export of a compliance report -> non-empty bytes.
    common::Result<std::vector<std::uint8_t>> exportPdf(
        const ComplianceReport& report);

    // (B) Word (docx) export of a compliance report -> non-empty bytes.
    common::Result<std::vector<std::uint8_t>> exportWord(
        const ComplianceReport& report);

    // (C) ReQIF export of requirements + trace links -> non-empty bytes whose
    //     content references the requirement ids and the trace links.
    common::Result<std::vector<std::uint8_t>> exportReqif(
        const std::vector<tracelink::Entity>& requirements,
        const std::vector<tracelink::Link>& links);

    // (D) result->requirement traceability: the requirement(s) a test result
    //     verifies (via trace links with relation 'verifies').
    common::Result<std::vector<std::string>> traceResultToRequirements(
        const std::string& resultId);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::assurecheck
