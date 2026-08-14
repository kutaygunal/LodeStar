// core/common/QualityScoring.cpp
// Gap-Fill RiskAI 1.7: shared requirement-quality scoring service.

#include "core/common/QualityScoring.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace lodestar::common {

namespace {

std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string lower(const std::string& s) {
    std::string out = s;
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

size_t wordCount(const std::string& s) {
    std::istringstream ss(s);
    std::string w;
    size_t n = 0;
    while (ss >> w) ++n;
    return n;
}

int countSub(const std::string& s, const std::string& sub) {
    std::string ls = lower(s), lsub = lower(sub);
    int n = 0;
    size_t pos = 0;
    while ((pos = ls.find(lsub, pos)) != std::string::npos) {
        ++n;
        pos += lsub.size();
    }
    return n;
}

bool hasVagueWord(const std::string& s) {
    static const char* kWords[] = {
        "stuff", "handle", "etc", "some", "thing", "things", "various",
        "maybe", "good", "fast", "quick", "nice", "properly", "roughly",
        "approximately", "about", "soon", "later", "many", "few", "big",
        "small", "large", "enough", "appropriate", "suitable", "whatever",
        "somehow", "somewhat", "kind of", "sort of", "tbd", "n/a"};
    std::string ls = lower(s);
    for (const char* w : kWords) {
        if (ls.find(w) != std::string::npos) return true;
    }
    return false;
}

bool hasUnitOrNumber(const std::string& s) {
    static const char* kUnits[] = {
        "km/h", "m/s", "hz", "mhz", "ghz", "khz", "db", "ms", "km", "kg",
        "volt", "amp", "watt", "percent", "degree", "second", "minute",
        "hour", "meter", "metre", "byte", "bit", "kbps", "mbps", "gbps"};
    std::string ls = lower(s);
    for (const char* u : kUnits) {
        if (ls.find(u) != std::string::npos) return true;
    }
    for (char c : s) {
        if (std::isdigit(static_cast<unsigned char>(c))) return true;
    }
    return false;
}

bool hasActionVerb(const std::string& s) {
    static const char* kVerbs[] = {
        "display", "show", "return", "compute", "calculate", "provide",
        "produce", "output", "send", "receive", "store", "record", "log",
        "detect", "measure", "report", "generate", "update", "set", "enable",
        "disable", "start", "stop", "validate", "verify", "check", "convert",
        "transmit", "indicate", "alert", "notify", "reject", "accept", "allow",
        "deny", "lock", "unlock", "open", "close"};
    std::string ls = lower(s);
    for (const char* v : kVerbs) {
        if (ls.find(v) != std::string::npos) return true;
    }
    return false;
}

int clamp100(int v) {
    if (v < 0) return 0;
    if (v > 100) return 100;
    return v;
}

}  // namespace

QualityScore scoreQuality(const std::string& raw) {
    std::string text = trim(raw);
    size_t wc = wordCount(text);
    int shall = countSub(text, "shall");
    bool hasShall = shall > 0;
    bool vague = hasVagueWord(text);
    bool measurable = hasUnitOrNumber(text);
    bool action = hasActionVerb(text);

    QualityScore s;

    // Clarity: formal "shall" + adequate length.
    s.clarity = 0;
    if (hasShall) s.clarity += 40;
    if (wc >= 5) s.clarity += 30;
    if (wc >= 8) s.clarity += 30;
    s.clarity = clamp100(s.clarity);

    // Testability: verifiable verb + measurable unit/number + formal marker.
    s.testability = 0;
    if (hasShall) s.testability += 30;
    if (measurable) s.testability += 40;
    if (action) s.testability += 30;
    s.testability = clamp100(s.testability);

    // Atomicity: single "shall", short scope, no conjunction.
    s.atomicity = 0;
    if (shall == 1) s.atomicity += 40;
    else if (shall == 0) s.atomicity += 20;
    if (wc <= 20) s.atomicity += 30;
    if (countSub(text, " and ") == 0 && countSub(text, " or ") == 0)
        s.atomicity += 10;
    s.atomicity = clamp100(s.atomicity);

    // Completeness: subject + verb + object, adequate detail.
    s.completeness = 0;
    if (hasShall) s.completeness += 40;
    if (wc >= 5) s.completeness += 30;
    if (wc >= 8) s.completeness += 30;
    s.completeness = clamp100(s.completeness);

    // Ambiguity: start high, penalize vague wording and informal phrasing.
    s.ambiguity = 100;
    if (vague) s.ambiguity -= 30;
    if (!hasShall) s.ambiguity -= 20;
    if (wc < 3) s.ambiguity -= 20;
    s.ambiguity = clamp100(s.ambiguity);

    s.overall = clamp100((s.clarity + s.testability + s.atomicity +
                          s.completeness + s.ambiguity) / 5);
    return s;
}

QualityResult scoreQualityWithFlags(const std::string& text) {
    QualityResult res;
    res.score = scoreQuality(text);
    const int kWeak = 60;

    auto add = [&](const char* dim, const char* reason, const char* sug) {
        res.flags.push_back({dim, reason, sug});
    };

    if (res.score.clarity < kWeak)
        add("clarity",
            "No formal \"shall\" statement or wording is unclear",
            "Rewrite as: \"The system shall <action> ...\"");
    if (res.score.testability < kWeak)
        add("testability",
            "No measurable unit or verifiable action verb found",
            "Add a measurable threshold, e.g. a unit or numeric bound");
    if (res.score.atomicity < kWeak)
        add("atomicity",
            "Multiple responsibilities or conjunction (and/or) present",
            "Split into one requirement per responsibility");
    if (res.score.completeness < kWeak)
        add("completeness",
            "Missing subject, verb, object or adequate detail",
            "Complete the sentence with a subject, verb, object and condition");
    if (res.score.ambiguity < kWeak)
        add("ambiguity",
            "Vague or non-verifiable wording present",
            "Replace vague terms with specific, measurable wording");
    return res;
}

}  // namespace lodestar::common
