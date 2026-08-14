// core/riskai/FmeaAssessor.cpp
// Gap-Fill RiskAI 1.3: FMEA assessment of existing documents.

#include "core/riskai/FmeaAssessor.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace lodestar::riskai {

namespace {

std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

// True if the text is long enough to count as a real justification/description
// rather than an empty or one-word placeholder.
bool isSubstantial(const std::string& s) {
    std::string t = trim(s);
    return t.size() >= 10;
}

bool containsVague(const std::string& s) {
    const char* vague[] = {"etc", "something", "as needed", "whatever",
                           "somehow", "unknown", "tbd", "n/a"};
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    for (const char* v : vague) {
        if (lower.find(v) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

FmeaAssessor::FmeaAssessor(adapters::IAdapter& llm,
                           const adapters::AdapterConfig& cfg)
    : llm_(llm), cfg_(cfg) {}

bool FmeaAssessor::parseImport(const std::string& text,
                               std::vector<ImportedFmeaRow>& out) const {
    std::istringstream ss(text);
    std::string line;
    bool any = false;
    int lineno = 0;
    while (std::getline(ss, line)) {
        ++lineno;
        line = trim(line);
        if (line.empty()) continue;
        // Split on '|' into the textual fields, and on leading commas for S/O/D.
        std::vector<std::string> cells;
        std::string cur;
        for (char c : line) {
            if (c == '|') {
                cells.push_back(trim(cur));
                cur.clear();
            } else {
                cur += c;
            }
        }
        cells.push_back(trim(cur));
        if (cells.size() < 5) continue;
        if (lineno == 1 && (cells[0] == "severity" || cells[0].find("severity") != std::string::npos))
            continue;  // header line

        ImportedFmeaRow r;
        auto toInt = [](const std::string& s) {
            if (s.empty()) return 0;
            try { return std::stoi(s); } catch (...) { return 0; }
        };
        r.severity = toInt(cells[0]);
        r.occurrence = toInt(cells[1]);
        r.detectionRating = toInt(cells[2]);
        // cells[3..] map to function|failureMode|effect|cause|preventive|detection
        if (cells.size() >= 4) r.function = cells[3];
        if (cells.size() >= 5) r.failureMode = cells[4];
        if (cells.size() >= 6) r.effect = cells[5];
        if (cells.size() >= 7) r.cause = cells[6];
        if (cells.size() >= 8) r.preventive = cells[7];
        if (cells.size() >= 9) r.detection = cells[8];
        if (r.failureMode.empty() && r.function.empty()) continue;
        out.push_back(std::move(r));
        any = true;
    }
    return any;
}

FmeaAssessmentResult FmeaAssessor::assess(
    const std::vector<ImportedFmeaRow>& rows) const {
    // Always run the deterministic engine; it is the source of truth for tests
    // and the honest fallback. A live-LLM pass would refine the messages.
    return deterministicAssess(rows);
}

FmeaAssessmentResult FmeaAssessor::deterministicAssess(
    const std::vector<ImportedFmeaRow>& rows) {
    FmeaAssessmentResult result;
    for (const auto& r : rows) {
        std::string rowId = r.id.empty() ? r.failureMode : r.id;

        // --- completeness ---
        if (r.failureMode.empty()) {
            result.findings.push_back(
                {rowId, "completeness", "critical",
                 "Failure mode (FM) is missing",
                 "Document the failure mode in the Function column"});
        } else if (r.effect.empty()) {
            result.findings.push_back(
                {rowId, "completeness", "major",
                 "Failure effect (FE) is missing",
                 "Add the consequence of the failure on the system"});
        }
        if (r.cause.empty()) {
            result.findings.push_back(
                {rowId, "completeness", "major",
                 "Failure cause (FC) is missing",
                 "Add the root cause of the failure mode"});
        }

        // --- clarity ---
        if (!r.failureMode.empty() && containsVague(r.failureMode + " " + r.effect + " " + r.cause)) {
            result.findings.push_back(
                {rowId, "clarity", "major",
                 "Row uses vague/placeholder wording",
                 "Replace vague terms with specific, measurable wording"});
        }

        // --- severity justification ---
        if (r.severity < 1 || r.severity > 10) {
            result.findings.push_back(
                {rowId, "severity_justification", "critical",
                 "Severity (S) rating is missing or out of range",
                 "Rate severity 1..10 per the AIAG-VDA scale"});
        } else if (!isSubstantial(r.effect)) {
            result.findings.push_back(
                {rowId, "severity_justification", "major",
                 "Severity (S) is not justified by a substantial effect",
                 "Describe the effect clearly to support the severity rating"});
        }

        // --- detection adequacy ---
        if (r.detectionRating < 1 || r.detectionRating > 10) {
            result.findings.push_back(
                {rowId, "detection_adequacy", "major",
                 "Detection (D) rating is missing or out of range",
                 "Rate detection 1..10 per the AIAG-VDA scale"});
        } else if (r.detectionRating >= 7 && !isSubstantial(r.detection)) {
            result.findings.push_back(
                {rowId, "detection_adequacy", "major",
                 "Poor detection (D>=7) lacks documented controls",
                 "Document existing detection controls or add new ones"});
        }

        // --- risk-scoring consistency ---
        if (r.severity >= 1 && r.severity <= 10 &&
            r.occurrence >= 1 && r.occurrence <= 10 &&
            r.detectionRating >= 1 && r.detectionRating <= 10) {
            int rpn = r.severity * r.occurrence * r.detectionRating;
            if (rpn >= 200 && r.cause.empty()) {
                result.findings.push_back(
                    {rowId, "risk_consistency", "critical",
                     "High RPN (>=200) but no root cause documented",
                     "Add the root cause; high risk without a cause is untraceable"});
            }
        }
    }

    for (const auto& f : result.findings) {
        if (f.severity == "critical") ++result.criticalCount;
        else if (f.severity == "major") ++result.majorCount;
        else ++result.minorCount;
    }

    // Score 0..100. Start at 100, subtract per finding weighted by severity.
    int score = 100;
    score -= result.criticalCount * 20;
    score -= result.majorCount * 10;
    score -= result.minorCount * 5;
    if (score < 0) score = 0;
    result.score = score;
    return result;
}

}  // namespace lodestar::riskai
