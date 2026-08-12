// core/testforge/CoberturaImport.cpp
#include "core/testforge/CoberturaImport.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>

#include "core/common/Sha256.h"
#include "core/common/Time.h"

namespace lodestar::testforge {

namespace {

// Returns the value of attribute `name="..."` in `tag`, or nullopt if the
// attribute isn't present. Deliberately minimal: Cobertura is a fixed,
// machine-generated format here (OpenCppCoverage's own output), not
// untrusted/adversarial XML, so a full XML parser would be overkill - this
// project has no XML-parsing dependency to reuse (OSLC's RDF/XML writer is
// output-only) and adding one for a single, well-known, regular tag shape
// isn't worth the new dependency.
std::optional<std::string> attr(const std::string& tag, const std::string& name) {
    const std::string needle = name + "=\"";
    size_t start = tag.find(needle);
    if (start == std::string::npos) return std::nullopt;
    start += needle.size();
    size_t end = tag.find('"', start);
    if (end == std::string::npos) return std::nullopt;
    return tag.substr(start, end - start);
}

// Normalizes a Cobertura filename (as OpenCppCoverage emits it - an
// absolute-ish path with backslashes, relative to the <source> root) to a
// repo-relative forward-slash path rooted at "core/", matching the `scope`
// convention documented in Coverage.h ("module:core/testforge/Coverage.cpp").
// Files outside core/ (e.g. vcpkg headers OpenCppCoverage still recorded
// because they matched --sources loosely) are returned unchanged; callers
// skip anything that doesn't start with "core/".
std::string normalizeFilename(std::string f) {
    for (char& c : f) {
        if (c == '\\') c = '/';
    }
    size_t core = f.find("core/");
    if (core != std::string::npos) return f.substr(core);
    return f;
}

}  // namespace

common::Result<std::vector<FileCoverage>> parseCoberturaReport(
    const std::string& xmlPath) {
    std::ifstream in(xmlPath, std::ios::binary);
    if (!in) {
        return common::Result<std::vector<FileCoverage>>::err(
            "cannot open Cobertura report: " + xmlPath);
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    const std::string xml = buf.str();

    // A report produced with --cover_children (the normal way to run this -
    // see ci/run_coverage.ps1) has one <package> per child process, i.e. one
    // per test binary, and the SAME source file shows up as a separate
    // <class> under every package that happens to execute any of it (a
    // shared file like core/common/Result.h can appear 40+ times in a
    // 50-binary suite). Naively taking the last <class> seen per filename
    // would silently discard most of the data. Aggregate per (filename,
    // line number) instead, OR-ing hit status across every occurrence: a
    // line is "executed" if ANY test binary executed it.
    std::map<std::string, std::map<int, bool>> byFile;

    size_t pos = 0;
    while (true) {
        size_t classStart = xml.find("<class ", pos);
        if (classStart == std::string::npos) break;
        size_t classTagEnd = xml.find('>', classStart);
        if (classTagEnd == std::string::npos) break;
        size_t classEnd = xml.find("</class>", classTagEnd);
        if (classEnd == std::string::npos) break;

        const std::string classTag = xml.substr(classStart, classTagEnd - classStart);
        const std::string body = xml.substr(classTagEnd, classEnd - classTagEnd);
        pos = classEnd + 8;  // strlen("</class>")

        auto filenameOpt = attr(classTag, "filename");
        if (!filenameOpt) continue;
        std::string filename = normalizeFilename(*filenameOpt);
        if (filename.rfind("core/", 0) != 0) continue;         // outside core/
        if (filename.rfind("core/test/", 0) == 0) continue;    // exclude test code itself

        auto& lines = byFile[filename];

        size_t linePos = 0;
        while (true) {
            size_t lineStart = body.find("<line ", linePos);
            if (lineStart == std::string::npos) break;
            size_t lineTagEnd = body.find("/>", lineStart);
            if (lineTagEnd == std::string::npos) break;
            const std::string lineTag =
                body.substr(lineStart, lineTagEnd - lineStart);
            linePos = lineTagEnd + 2;

            auto numberOpt = attr(lineTag, "number");
            auto hitsOpt = attr(lineTag, "hits");
            if (!numberOpt || !hitsOpt) continue;
            int number = std::atoi(numberOpt->c_str());
            bool hit = std::atoi(hitsOpt->c_str()) > 0;
            // OR-combine: once a line is seen executed by any binary, later
            // occurrences (even hits=0 from a binary that never reached it)
            // must not un-cover it.
            bool& seen = lines[number];
            seen = seen || hit;
        }
    }

    std::vector<FileCoverage> out;
    for (const auto& [filename, lines] : byFile) {
        if (lines.empty()) continue;
        FileCoverage fc;
        fc.filename = filename;
        fc.statementsTotal = static_cast<int>(lines.size());
        for (const auto& [number, hit] : lines) {
            if (hit) ++fc.statementsExecuted;
        }
        out.push_back(std::move(fc));
    }

    if (out.empty()) {
        return common::Result<std::vector<FileCoverage>>::err(
            "Cobertura report parsed but no core/ source files with line data "
            "were found: " +
            xmlPath);
    }
    return common::Result<std::vector<FileCoverage>>::ok(std::move(out));
}

common::Result<int> importCoberturaReport(const std::string& xmlPath,
                                          const std::string& runId,
                                          CoverageDao& dao) {
    auto parsed = parseCoberturaReport(xmlPath);
    if (parsed.failed()) {
        return common::Result<int>::err(parsed.error());
    }
    const std::string recordedAt = common::nowIso();
    int count = 0;
    for (const auto& fc : parsed.value()) {
        CoverageResult r;
        const std::string scope = "module:" + fc.filename;
        // Deterministic id from (runId, scope) so re-importing the same
        // report under the same runId updates in place (CoverageDao::save
        // is an upsert-by-id) instead of accumulating duplicate rows.
        r.id = common::sha256Hex("cobertura|" + runId + "|" + scope);
        r.runId = runId;
        r.scope = scope;
        r.statementsExecuted = fc.statementsExecuted;
        r.statementsTotal = fc.statementsTotal;
        // Not measured by this import path - see file header. Left at 0
        // rather than fabricated.
        r.decisionsTaken = 0;
        r.decisionsTotal = 0;
        r.conditionsSatisfied = 0;
        r.conditionsTotal = 0;
        r.recordedAt = recordedAt;
        auto saved = dao.save(r);
        if (saved.failed()) {
            return common::Result<int>::err(saved.error());
        }
        ++count;
    }
    return common::Result<int>::ok(count);
}

}  // namespace lodestar::testforge
