#pragma once
// core/assurecheck/CertReportService.h
// S2 Phase 8: certification-ready reporting + traceability.
// S3 Phase 4: depth pass -- multi-page/paginated PDF, complete-OOXML Word,
// schema-complete ReQIF, evidence + workflow audit trail embedded in every
// export. See PLAN.md ("Phase 4 scope brief") for the gap analysis this
// phase closes.
//
// Produces PDF / Word (docx) / ReQIF exports of compliance reports and of
// requirements + trace links, and resolves test-case->requirement
// traceability (which requirement(s) a TraceLink test_case verifies via
// trace links).
//
// Contract written by the scrum-master in docs/s2-phase8-test.md and
// core/test/s2_phase8_tests.cpp (S2 Phase 8); extended, not replaced, by
// core/test/s3_phase4_tests.cpp (S3 Phase 4).

#include <cstdint>
#include <string>
#include <vector>

#include "core/assurecheck/ReportService.h"
#include "core/assurecheck/WorkflowService.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/tracelink/Types.h"

namespace lodestar::assurecheck {

// Certification-ready export + traceability service (S2 Phase 8, deepened in
// S3 Phase 4).
class CertReportService {
public:
    explicit CertReportService(persistence::Database& db);

    // (A) PDF export of a compliance report: multi-page, titled, tabular
    // pass/fail rows with evidence, page numbers, and (when rows carry a
    // resultId) a review/approval audit-trail section per row.
    common::Result<std::vector<std::uint8_t>> exportPdf(
        const ComplianceReport& report);

    // (B) Word (docx) export of a compliance report: a complete OOXML
    // package (styles/settings/fontTable/docProps), a real table for the
    // pass/fail rows (not one paragraph per line), and the same evidence +
    // audit-trail content as the PDF export.
    common::Result<std::vector<std::uint8_t>> exportWord(
        const ComplianceReport& report);

    // (C) ReQIF export of requirements + trace links -> non-empty bytes
    // whose content references the requirement ids and the trace links, with
    // a complete SPEC-TYPES/DATATYPES section so every SPEC-OBJECT-TYPE-REF
    // and SPEC-RELATION-TYPE-REF resolves (schema-valid for DOORS/Polarion/
    // Codebeamer import, not just well-formed XML).
    common::Result<std::vector<std::uint8_t>> exportReqif(
        const std::vector<tracelink::Entity>& requirements,
        const std::vector<tracelink::Link>& links);

    // (D) test_case->requirement traceability: the requirement(s) a
    // TraceLink test_case entity verifies (via 'verifies' trace links).
    // Renamed from the S2 Phase 8 `traceResultToRequirements` to match what
    // it actually does -- see PLAN.md Phase 4 notes (item 6): TestForge's
    // TestRun/TestProcedure model has no foreign key to TraceLink test_case
    // entities in the current schema (migrations 002, 020), so a real
    // TestForge-run-based resolver would have nothing to resolve through;
    // renaming to an honest name was judged better than fabricating a link
    // that doesn't exist in the data model.
    common::Result<std::vector<std::string>> verifiedRequirementsForTestCase(
        const std::string& testCaseId);

    // Deprecated alias for verifiedRequirementsForTestCase, kept so the
    // existing S2 Phase 8 contract (core/test/s2_phase8_tests.cpp) keeps
    // compiling and passing unchanged. New callers should use
    // verifiedRequirementsForTestCase directly.
    common::Result<std::vector<std::string>> traceResultToRequirements(
        const std::string& resultId);

private:
    // Collects the workflow audit trail for every row of `report` (by
    // row.resultId), one entry list per row, same order/length as
    // report.rows. Rows with no resultId (e.g. hand-built reports that never
    // went through ReportService::buildReport) get an empty list, not an
    // error -- an export without an audit trail is still a valid export.
    std::vector<std::vector<AuditEntry>> gatherAuditByRow(
        const ComplianceReport& report);

    persistence::Database& db_;
    WorkflowService workflow_;
};

}  // namespace lodestar::assurecheck
