// core/test/s3_phase4_tests.cpp
// ---------------------------------------------------------------------------
// Sprint 3 Phase 4 (S3.4): certification-grade exports -- depth pass.
//
// Extends (does not replace) core/test/s2_phase8_tests.cpp, which only
// asserted "non-empty bytes" / substring presence. These tests assert real
// structural properties instead, per PLAN.md Phase 4 brief item 7:
//   - PDF: real page count (via a hand-rolled xref/Pages-object reader) and
//     extractable text (via a hand-rolled Tj-literal scanner), across a
//     report large enough to force pagination.
//   - Word (docx): the zip actually contains every OOXML part a real docx
//     needs, and word/document.xml has a real <w:tbl> (not one paragraph
//     per line) with the expected row count.
//   - ReQIF: SPEC-TYPES/DATATYPES are complete -- every *-REF in the
//     document resolves to a matching IDENTIFIER definition (checked with a
//     small hand-rolled identifier/reference collector, matching this
//     project's existing precedent of hand-rolling small targeted parsers --
//     see core/testforge/CoberturaImport.cpp).
//   - Evidence + workflow audit trail actually appear in the PDF and Word
//     output for a row that went through a real WorkflowService transition.
//   - verifiedRequirementsForTestCase / the deprecated traceResultToRequirements
//     alias agree (S2 Phase 8 back-compat, S3 Phase 4 rename).
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / S1/S2/S3
// phases.
// ---------------------------------------------------------------------------

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/assurecheck/AssureCheckService.h"
#include "core/assurecheck/CertReportService.h"
#include "core/assurecheck/ComplianceEngine.h"
#include "core/assurecheck/ReportService.h"
#include "core/assurecheck/WorkflowService.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/Types.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace ac = lodestar::assurecheck;
namespace p  = lodestar::persistence;
namespace tl = lodestar::tracelink;

namespace {

// ---------------------------------------------------------------------------
// Lightweight test harness (matches the rest of the suite).
// ---------------------------------------------------------------------------
class Harness {
public:
    explicit Harness(const char* name) : name_(name) {}

    void section(const char* s) { std::printf("\n-- %s --\n", s); }

    void check(bool cond, const char* what) {
        if (cond) {
            std::printf("  [PASS] %s\n", what);
        } else {
            std::printf("  [FAIL] %s\n", what);
            ++failures_;
        }
    }

    int failures() const { return failures_; }
    const char* name() const { return name_; }

private:
    const char* name_;
    int failures_ = 0;
};

// ---------------------------------------------------------------------------
// Hand-rolled PDF readers -- deliberately minimal, matching what this
// project's own writer produces (uncompressed content streams, literal
// strings escaped per PDF spec). Not a general PDF parser.
// ---------------------------------------------------------------------------

// Reads the /Count on the /Type /Pages object -> total page count, or -1.
int pdfPageCount(const std::vector<std::uint8_t>& bytes) {
    std::string s(bytes.begin(), bytes.end());
    size_t p = s.find("/Type /Pages");
    if (p == std::string::npos) return -1;
    size_t c = s.find("/Count", p);
    if (c == std::string::npos) return -1;
    c += 6;
    while (c < s.size() && s[c] == ' ') ++c;
    size_t end = c;
    while (end < s.size() && std::isdigit(static_cast<unsigned char>(s[end]))) ++end;
    if (end == c) return -1;
    return std::atoi(s.substr(c, end - c).c_str());
}

// Extracts every literal-string operand of a Tj show-text operator, honoring
// PDF's backslash-escaping of '(' ')' '\\' inside literal strings.
std::vector<std::string> extractPdfText(const std::vector<std::uint8_t>& bytes) {
    std::string s(bytes.begin(), bytes.end());
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        size_t open = s.find('(', i);
        if (open == std::string::npos) break;
        std::string text;
        size_t j = open + 1;
        while (j < s.size()) {
            char c = s[j];
            if (c == '\\' && j + 1 < s.size()) {
                text += s[j + 1];
                j += 2;
                continue;
            }
            if (c == ')') { ++j; break; }
            text += c;
            ++j;
        }
        size_t after = s.find_first_not_of(' ', j);
        if (after != std::string::npos && s.compare(after, 2, "Tj") == 0) {
            out.push_back(text);
        }
        i = j;
    }
    return out;
}

bool pdfTextContains(const std::vector<std::string>& lines,
                     const std::string& needle) {
    for (const auto& l : lines) {
        if (l.find(needle) != std::string::npos) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Hand-rolled store-method zip reader -- matches this project's own zip
// writer (buildZip in CertReportService.cpp): no compression, local file
// headers followed immediately by raw data, in file order.
// ---------------------------------------------------------------------------
std::map<std::string, std::string> parseStoreZip(
    const std::vector<std::uint8_t>& bytes) {
    std::map<std::string, std::string> out;
    auto u16 = [&](size_t p) -> unsigned {
        return static_cast<unsigned>(bytes[p]) |
               (static_cast<unsigned>(bytes[p + 1]) << 8);
    };
    auto u32 = [&](size_t p) -> unsigned {
        return static_cast<unsigned>(bytes[p]) |
               (static_cast<unsigned>(bytes[p + 1]) << 8) |
               (static_cast<unsigned>(bytes[p + 2]) << 16) |
               (static_cast<unsigned>(bytes[p + 3]) << 24);
    };
    size_t pos = 0;
    while (pos + 30 <= bytes.size()) {
        if (u32(pos) != 0x04034b50u) break;  // local file header signature
        const unsigned compSize = u32(pos + 18);
        const unsigned nameLen = u16(pos + 26);
        const unsigned extraLen = u16(pos + 28);
        const size_t nameStart = pos + 30;
        if (nameStart + nameLen + extraLen + compSize > bytes.size()) break;
        std::string name(reinterpret_cast<const char*>(&bytes[nameStart]),
                         nameLen);
        const size_t dataStart = nameStart + nameLen + extraLen;
        std::string data(reinterpret_cast<const char*>(&bytes[dataStart]),
                         compSize);
        out[name] = std::move(data);
        pos = dataStart + compSize;
    }
    return out;
}

std::size_t countOccurrences(const std::string& haystack,
                             const std::string& needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

// ---------------------------------------------------------------------------
// Hand-rolled ReQIF identifier/reference collector -- validates that every
// SPEC-OBJECT-TYPE-REF / SPEC-RELATION-TYPE-REF / ATTRIBUTE-DEFINITION-*-REF
// / DATATYPE-DEFINITION-*-REF resolves to a matching IDENTIFIER="..." def,
// i.e. a round-trip schema-completeness check without a full XML library.
// ---------------------------------------------------------------------------
std::set<std::string> collectIdentifiers(const std::string& xml,
                                         const std::string& tag) {
    std::set<std::string> ids;
    const std::string needle = "<" + tag + " ";
    std::size_t pos = 0;
    while ((pos = xml.find(needle, pos)) != std::string::npos) {
        const std::size_t tagEnd = xml.find('>', pos);
        const std::size_t idAttr = xml.find("IDENTIFIER=\"", pos);
        if (idAttr == std::string::npos ||
            (tagEnd != std::string::npos && idAttr > tagEnd)) {
            pos += needle.size();
            continue;
        }
        const std::size_t valStart = idAttr + std::strlen("IDENTIFIER=\"");
        const std::size_t valEnd = xml.find('"', valStart);
        ids.insert(xml.substr(valStart, valEnd - valStart));
        pos = valEnd;
    }
    return ids;
}

std::vector<std::string> collectRefs(const std::string& xml,
                                     const std::string& tag) {
    std::vector<std::string> refs;
    const std::string open = "<" + tag + ">";
    const std::string close = "</" + tag + ">";
    std::size_t pos = 0;
    while ((pos = xml.find(open, pos)) != std::string::npos) {
        const std::size_t start = pos + open.size();
        const std::size_t end = xml.find(close, start);
        if (end == std::string::npos) break;
        refs.push_back(xml.substr(start, end - start));
        pos = end + close.size();
    }
    return refs;
}

// ---------------------------------------------------------------------------
// Report fixtures.
// ---------------------------------------------------------------------------
ac::ComplianceReport makeReport(int rowCount) {
    ac::ComplianceReport rep;
    rep.standardCode = "DO-178C";
    rep.standardName = "Software Considerations in Airborne Systems";
    rep.dalLevel = "A";
    for (int i = 0; i < rowCount; ++i) {
        ac::ReportRow r;
        r.itemCode = "A2-" + std::to_string(i + 1);
        r.objective = "Checklist objective number " + std::to_string(i + 1);
        r.dalLevel = "A";
        r.status = (i % 5 == 0) ? "FAIL" : "PASS";
        r.evidence = "test_case:tc" + std::to_string(i + 1);
        rep.rows.push_back(r);
    }
    rep.coverage.total = rowCount;
    rep.coverage.applicable = rowCount;
    rep.coverage.pass = rowCount - rowCount / 5;
    rep.coverage.fail = rowCount / 5;
    rep.coverage.percent =
        rowCount > 0 ? (rep.coverage.pass * 100 / rowCount) : 0;
    return rep;
}

// ---------------------------------------------------------------------------
// T1. PDF export: real multi-page structure + extractable text.
// ---------------------------------------------------------------------------
void testPdfStructure(Harness& h, ac::CertReportService& svc) {
    h.section("T1. PDF export: multi-page structure, extractable text");

    const ac::ComplianceReport report = makeReport(70);  // forces pagination
    auto pdf = svc.exportPdf(report);
    h.check(pdf.isOk(), "exportPdf(large report) ok");
    if (!pdf.isOk()) return;

    const int pages = pdfPageCount(pdf.value());
    h.check(pages > 1, "a 70-row report paginates to more than one PDF page");

    const auto texts = extractPdfText(pdf.value());
    h.check(!texts.empty(), "PDF content streams contain extractable Tj text");
    h.check(pdfTextContains(texts, "Compliance Report - DO-178C"),
           "extracted text includes the report title");
    h.check(pdfTextContains(texts, "A2-1"),
           "extracted text includes the first item code");
    h.check(pdfTextContains(texts, "A2-70"),
           "extracted text includes the last item code (proves pagination "
           "didn't drop rows)");
    h.check(pdfTextContains(texts, "Page 1 of " + std::to_string(pages)),
           "first page has a 'Page 1 of N' footer");
}

// ---------------------------------------------------------------------------
// T2. Word (docx) export: real OOXML package with a real table.
// ---------------------------------------------------------------------------
void testWordStructure(Harness& h, ac::CertReportService& svc) {
    h.section("T2. Word export: complete OOXML package, real table");

    const ac::ComplianceReport report = makeReport(3);
    auto doc = svc.exportWord(report);
    h.check(doc.isOk(), "exportWord(report) ok");
    if (!doc.isOk()) return;

    const auto parts = parseStoreZip(doc.value());
    static const char* kRequiredParts[] = {
        "[Content_Types].xml",       "_rels/.rels",
        "word/document.xml",         "word/_rels/document.xml.rels",
        "word/styles.xml",           "word/settings.xml",
        "word/fontTable.xml",        "docProps/core.xml",
        "docProps/app.xml",
    };
    for (const char* name : kRequiredParts) {
        h.check(parts.count(name) == 1,
               (std::string("docx package contains ") + name).c_str());
    }

    auto it = parts.find("word/document.xml");
    h.check(it != parts.end(), "word/document.xml present");
    if (it == parts.end()) return;
    const std::string& xml = it->second;

    h.check(countOccurrences(xml, "<w:tbl>") == 1,
           "document.xml has exactly one real table (w:tbl), not per-line "
           "paragraphs");
    // header row + 3 data rows.
    h.check(countOccurrences(xml, "<w:tr>") == 4,
           "table has a header row plus one row per checklist item");
    h.check(xml.find("A2-1") != std::string::npos,
           "table contains the first item code");
    h.check(xml.find("test_case:tc1") != std::string::npos,
           "table contains the evidence column");

    auto rootRels = parts.find("_rels/.rels");
    h.check(rootRels != parts.end() &&
               rootRels->second.find("docProps/core.xml") != std::string::npos,
           "_rels/.rels references docProps/core.xml");

    auto contentTypes = parts.find("[Content_Types].xml");
    h.check(contentTypes != parts.end() &&
               contentTypes->second.find("word/styles.xml") !=
                   std::string::npos,
           "[Content_Types].xml declares word/styles.xml");
}

// ---------------------------------------------------------------------------
// T3. ReQIF export: SPEC-TYPES/DATATYPES complete, every *-REF resolves.
// ---------------------------------------------------------------------------
void testReqifSchema(Harness& h, ac::CertReportService& svc) {
    h.section("T3. ReQIF export: complete SPEC-TYPES, every *-REF resolves");

    tl::Entity req1;
    req1.id = "r1";
    req1.externalId = "REQ-100";
    req1.name = "Position output";
    req1.text = "The system shall provide GNSS position output.";

    tl::Entity req2;
    req2.id = "r2";
    req2.externalId = "REQ-200";
    req2.name = "Altitude accuracy";
    req2.text = "The system shall report altitude within tolerance.";

    tl::Link link;
    link.id = "tl1";
    link.sourceId = "REQ-100";
    link.targetId = "REQ-200";
    link.relation = "verifies";

    auto reqif = svc.exportReqif({req1, req2}, {link});
    h.check(reqif.isOk(), "exportReqif(reqs, links) ok");
    if (!reqif.isOk()) return;

    const std::string xml(reqif.value().begin(), reqif.value().end());
    h.check(xml.find("<DATATYPES>") != std::string::npos,
           "reqif has a DATATYPES section");
    h.check(xml.find("<SPEC-TYPES>") != std::string::npos,
           "reqif has a SPEC-TYPES section");

    const auto objTypeIds = collectIdentifiers(xml, "SPEC-OBJECT-TYPE");
    const auto relTypeIds = collectIdentifiers(xml, "SPEC-RELATION-TYPE");
    const auto attrDefIds =
        collectIdentifiers(xml, "ATTRIBUTE-DEFINITION-STRING");
    const auto datatypeIds =
        collectIdentifiers(xml, "DATATYPE-DEFINITION-STRING");

    const auto objTypeRefs = collectRefs(xml, "SPEC-OBJECT-TYPE-REF");
    const auto relTypeRefs = collectRefs(xml, "SPEC-RELATION-TYPE-REF");
    const auto attrDefRefs =
        collectRefs(xml, "ATTRIBUTE-DEFINITION-STRING-REF");
    const auto datatypeRefs =
        collectRefs(xml, "DATATYPE-DEFINITION-STRING-REF");

    h.check(!objTypeRefs.empty(), "at least one SPEC-OBJECT-TYPE-REF present");
    h.check(!relTypeRefs.empty(),
           "at least one SPEC-RELATION-TYPE-REF present");
    h.check(!attrDefRefs.empty(),
           "at least one ATTRIBUTE-DEFINITION-STRING-REF present");
    h.check(!datatypeRefs.empty(),
           "at least one DATATYPE-DEFINITION-STRING-REF present");

    bool allObjTypeRefsResolve = true;
    for (const auto& r : objTypeRefs) {
        if (!objTypeIds.count(r)) allObjTypeRefsResolve = false;
    }
    h.check(allObjTypeRefsResolve,
           "every SPEC-OBJECT-TYPE-REF resolves to a defined SPEC-OBJECT-TYPE");

    bool allRelTypeRefsResolve = true;
    for (const auto& r : relTypeRefs) {
        if (!relTypeIds.count(r)) allRelTypeRefsResolve = false;
    }
    h.check(allRelTypeRefsResolve,
           "every SPEC-RELATION-TYPE-REF resolves to a defined "
           "SPEC-RELATION-TYPE");

    bool allAttrDefRefsResolve = true;
    for (const auto& r : attrDefRefs) {
        if (!attrDefIds.count(r)) allAttrDefRefsResolve = false;
    }
    h.check(allAttrDefRefsResolve,
           "every ATTRIBUTE-DEFINITION-STRING-REF resolves to a defined "
           "ATTRIBUTE-DEFINITION-STRING");

    bool allDatatypeRefsResolve = true;
    for (const auto& r : datatypeRefs) {
        if (!datatypeIds.count(r)) allDatatypeRefsResolve = false;
    }
    h.check(allDatatypeRefsResolve,
           "every DATATYPE-DEFINITION-STRING-REF resolves to a defined "
           "DATATYPE-DEFINITION-STRING");

    h.check(xml.find("REQ-100") != std::string::npos,
           "reqif content references requirement id REQ-100");
    h.check(xml.find("tl1") != std::string::npos,
           "reqif content references the trace link id tl1");
}

// ---------------------------------------------------------------------------
// T4. Evidence + workflow audit trail actually appear in the exports.
// ---------------------------------------------------------------------------
void testEvidenceAndAuditTrail(Harness& h, p::Database& db,
                               ac::CertReportService& svc) {
    h.section("T4. Evidence + workflow audit trail in PDF/Word output");

    ac::AssureCheckService assure(db);
    auto seeded = assure.seedStandards();
    h.check(seeded.isOk(), "seedStandards() ok");
    if (seeded.failed()) return;

    auto items = assure.checklistFor("DO-178C");
    h.check(items.isOk() && !items.value().empty(),
           "checklistFor(DO-178C) returns items");
    if (items.failed() || items.value().empty()) return;
    const auto& item = items.value().front();

    const std::string resultId = "s3p4-chk1";
    db.execute(
        "INSERT INTO assurance_checks (id, standard_id, item_id, item_code, "
        "status, dal_level, evidence, detail, checked_at) VALUES ('" +
        resultId + "','" + item.standardId + "','" + item.id + "','" +
        item.itemCode +
        "','PASS','A','test_case:tc1','','2026-01-01T00:00:00');");

    ac::WorkflowService workflow(db);
    auto submitted = workflow.submitForReview(resultId, "alice.reviewer");
    h.check(submitted.isOk(), "submitForReview(alice.reviewer) ok");
    auto approved = workflow.approve(resultId, "bob.approver");
    h.check(approved.isOk(), "approve(bob.approver) ok");

    ac::CheckResult result;
    result.id = resultId;
    result.standardCode = "DO-178C";
    result.itemCode = item.itemCode;
    result.itemId = item.id;
    result.status = ac::CheckStatus::Pass;
    result.dalLevel = "A";
    result.evidence.push_back({"test_case", "tc1"});

    ac::ReportService reportSvc(db);
    auto report = reportSvc.buildReport("DO-178C", "A", {result});
    h.check(report.isOk(), "buildReport(DO-178C, A, [result]) ok");
    if (report.failed()) return;
    h.check(!report.value().rows.empty() &&
               report.value().rows.front().resultId == resultId,
           "the built report row carries the real resultId through from "
           "CheckResult.id");

    auto pdf = svc.exportPdf(report.value());
    h.check(pdf.isOk(), "exportPdf(report-with-audit-trail) ok");
    if (pdf.isOk()) {
        const auto texts = extractPdfText(pdf.value());
        h.check(pdfTextContains(texts, "test_case:tc1"),
               "PDF text includes the row's evidence");
        h.check(pdfTextContains(texts, "alice.reviewer"),
               "PDF text includes the reviewing actor from the audit trail");
        h.check(pdfTextContains(texts, "bob.approver"),
               "PDF text includes the approving actor from the audit trail");
    }

    auto doc = svc.exportWord(report.value());
    h.check(doc.isOk(), "exportWord(report-with-audit-trail) ok");
    if (doc.isOk()) {
        const auto parts = parseStoreZip(doc.value());
        auto it = parts.find("word/document.xml");
        h.check(it != parts.end(), "word/document.xml present");
        if (it != parts.end()) {
            h.check(it->second.find("test_case:tc1") != std::string::npos,
                   "Word document includes the row's evidence");
            h.check(it->second.find("alice.reviewer") != std::string::npos,
                   "Word document includes the reviewing actor");
            h.check(it->second.find("bob.approver") != std::string::npos,
                   "Word document includes the approving actor");
        }
    }
}

// ---------------------------------------------------------------------------
// T5. verifiedRequirementsForTestCase / deprecated traceResultToRequirements
//     alias agree (S3 Phase 4 rename, S2 Phase 8 back-compat).
// ---------------------------------------------------------------------------
void testTraceabilityRename(Harness& h, ac::CertReportService& svc) {
    h.section("T5. verifiedRequirementsForTestCase + deprecated alias agree");

    auto viaNewName = svc.verifiedRequirementsForTestCase("tc1");
    h.check(viaNewName.isOk(), "verifiedRequirementsForTestCase(\"tc1\") ok");
    if (viaNewName.isOk()) {
        h.check(viaNewName.value().size() == 2,
               "test case tc1 verifies 2 requirements via the new name");
    }

    auto viaOldName = svc.traceResultToRequirements("tc1");
    h.check(viaOldName.isOk(),
           "deprecated traceResultToRequirements(\"tc1\") still ok");
    if (viaOldName.isOk() && viaNewName.isOk()) {
        h.check(viaOldName.value() == viaNewName.value(),
               "the deprecated alias returns exactly what the new name "
               "returns (non-breaking rename)");
    }

    auto none = svc.verifiedRequirementsForTestCase("tc_missing");
    h.check(none.isOk() && none.value().empty(),
           "a test case with no verifying links returns an empty list, not "
           "an error");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::string migrationsDir = LODESTAR_MIGRATIONS_DIR;
    if (argc > 1) {
        migrationsDir = argv[1];
    }

    const std::string dbPath = "lodestar_s3_phase4_tests.db";
    std::remove(dbPath.c_str());
    std::remove((dbPath + "-wal").c_str());
    std::remove((dbPath + "-shm").c_str());

    p::Database db;
    auto open = db.open(dbPath);
    if (open.failed()) {
        std::fprintf(stderr, "S3 PHASE4 TESTS FAIL: open db: %s\n",
                     open.error().c_str());
        return 1;
    }

    p::MigrationRunner runner(db);
    auto mig = runner.run(migrationsDir);
    if (mig.failed()) {
        std::fprintf(stderr, "S3 PHASE4 TESTS FAIL: migrate: %s\n",
                     mig.error().c_str());
        db.close();
        std::remove(dbPath.c_str());
        return 1;
    }

    // Seed T5's traceability data (same fixture shape as S2 Phase 8).
    db.execute("INSERT INTO requirements (id, name) VALUES ('req1', 'Req 1');");
    db.execute("INSERT INTO requirements (id, name) VALUES ('req2', 'Req 2');");
    db.execute("INSERT INTO test_cases (id, name, result_status) "
               "VALUES ('tc1', 'TC 1', 'Passed');");
    db.execute("INSERT INTO trace_links (id, source_type, source_id, "
               "target_type, target_id, relation, status) "
               "VALUES ('tl1', 'test_case', 'tc1', 'requirement', 'req1', "
               "'verifies', 'Active');");
    db.execute("INSERT INTO trace_links (id, source_type, source_id, "
               "target_type, target_id, relation, status) "
               "VALUES ('tl2', 'test_case', 'tc1', 'requirement', 'req2', "
               "'verifies', 'Active');");

    ac::CertReportService svc(db);

    Harness h("S3 Phase 4 certification-grade exports");
    std::printf("S3 PHASE 4 CERTIFICATION-GRADE EXPORTS TESTS (schema v%d)\n",
               mig.value());

    testPdfStructure(h, svc);
    testWordStructure(h, svc);
    testReqifSchema(h, svc);
    testEvidenceAndAuditTrail(h, db, svc);
    testTraceabilityRename(h, svc);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());

    db.close();
    std::remove(dbPath.c_str());
    std::remove((dbPath + "-wal").c_str());
    std::remove((dbPath + "-shm").c_str());

    return h.failures() == 0 ? 0 : 1;
}
