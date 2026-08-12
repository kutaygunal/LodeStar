// core/riskai/RequirementQualityService.cpp
// S2 Phase 13: AI quality scoring on requirements.

#include "core/riskai/RequirementQualityService.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace lodestar::riskai {

namespace {

// Trim leading/trailing whitespace.
std::string trim(const std::string& s) {
    size_t b = 0;
    size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

// Lowercase a copy of the string.
std::string lower(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// Count whitespace-separated words.
size_t wordCount(const std::string& s) {
    std::istringstream ss(s);
    std::string w;
    size_t n = 0;
    while (ss >> w) ++n;
    return n;
}

// Count occurrences of a substring (case-insensitive).
int countSub(const std::string& s, const std::string& sub) {
    std::string ls = lower(s);
    std::string lsub = lower(sub);
    int n = 0;
    size_t pos = 0;
    while ((pos = ls.find(lsub, pos)) != std::string::npos) {
        ++n;
        pos += lsub.size();
    }
    return n;
}

// Whether the text contains any vague / non-verifiable wording.
bool hasVagueWord(const std::string& s) {
    static const char* kWords[] = {
        "stuff", "handle", "etc", "some", "thing", "things", "various",
        "maybe", "good", "fast", "quick", "nice", "properly", "roughly",
        "approximately", "about", "soon", "later", "many", "few", "big",
        "small", "large", "enough", "appropriate", "suitable", "etc.",
        "and so on", "whatever", "somehow", "somewhat", "kind of", "sort of"
    };
    std::string ls = lower(s);
    for (const char* w : kWords) {
        if (ls.find(w) != std::string::npos) return true;
    }
    return false;
}

// Whether the text contains a measurable unit or a numeric value.
bool hasUnitOrNumber(const std::string& s) {
    static const char* kUnits[] = {
        "km/h", "m/s", "hz", "mhz", "ghz", "khz", "db", "ms", "km", "kg",
        "volt", "amp", "watt", "percent", "degree", "second", "minute",
        "hour", "meter", "metre", "byte", "bit", "kbps", "mbps", "gbps"
    };
    std::string ls = lower(s);
    for (const char* u : kUnits) {
        if (ls.find(u) != std::string::npos) return true;
    }
    for (char c : s) {
        if (std::isdigit(static_cast<unsigned char>(c))) return true;
    }
    return false;
}

// Whether the text contains an action / verifiable verb.
bool hasActionVerb(const std::string& s) {
    static const char* kVerbs[] = {
        "display", "show", "return", "compute", "calculate", "provide",
        "produce", "output", "send", "receive", "store", "record", "log",
        "detect", "measure", "report", "generate", "update", "set", "enable",
        "disable", "start", "stop", "validate", "verify", "check", "convert",
        "transmit", "display", "indicate", "alert", "notify", "reject",
        "accept", "allow", "deny", "lock", "unlock", "open", "close"
    };
    std::string ls = lower(s);
    for (const char* v : kVerbs) {
        if (ls.find(v) != std::string::npos) return true;
    }
    return false;
}

// Clamp an int into [0,100].
int clamp100(int v) {
    if (v < 0) return 0;
    if (v > 100) return 100;
    return v;
}

// Extract the model's generated text from the invoke() reply Json. The reply
// is either a parsed object (Ollama: {"response": "..."}) or a raw string.
std::string extractReplyText(const lodestar::Json& reply) {
    if (reply.isString()) return reply.asString();
    if (reply.isObject() && reply.has("response")) {
        const lodestar::Json& r = reply.at("response");
        if (r.isString()) return r.asString();
    }
    return "";
}

// Parse a JSON object of quality scores from the reply text. Returns false if
// the text does not contain a parseable object with at least one numeric key.
bool parseJsonScores(const std::string& text, QualityScore& out) {
    size_t open = text.find('{');
    if (open == std::string::npos) return false;
    int depth = 0;
    size_t close = std::string::npos;
    for (size_t i = open; i < text.size(); ++i) {
        char c = text[i];
        if (c == '{') ++depth;
        else if (c == '}') {
            --depth;
            if (depth == 0) { close = i; break; }
        }
    }
    if (close == std::string::npos) return false;
    std::string sub = text.substr(open, close - open + 1);
    lodestar::Json obj;
    try {
        obj = lodestar::Json::parse(sub);
    } catch (...) {
        return false;
    }
    if (!obj.isObject()) return false;

    auto readInt = [&](const char* key, int& target) -> bool {
        if (!obj.has(key)) return false;
        const lodestar::Json& v = obj.at(key);
        if (!v.isNumber()) return false;
        target = clamp100(static_cast<int>(v.asNumber()));
        return true;
    };

    bool any = false;
    if (readInt("clarity", out.clarity)) any = true;
    if (readInt("testability", out.testability)) any = true;
    if (readInt("atomicity", out.atomicity)) any = true;
    if (readInt("completeness", out.completeness)) any = true;
    if (readInt("ambiguity", out.ambiguity)) any = true;
    if (readInt("overall", out.overall)) any = true;
    return any;
}

}  // namespace

RequirementQualityService::RequirementQualityService(
    adapters::IAdapter& llm, const adapters::AdapterConfig& cfg)
    : llm_(llm), cfg_(cfg) {}

std::string RequirementQualityService::buildPrompt(
    const tracelink::Entity& req) const {
    std::string sys = cfg_.param("system",
        "You are a requirements quality engineer. Given a requirement, score it "
        "on five dimensions (clarity, testability, atomicity, completeness, "
        "ambiguity) each 0-100, plus an overall 0-100. Reply with ONLY a JSON "
        "object with keys \"clarity\", \"testability\", \"atomicity\", "
        "\"completeness\", \"ambiguity\", \"overall\".");
    std::string body = req.text.empty() ? req.name : req.text;
    return sys + "\n\nRequirement: " + body;
}

bool RequirementQualityService::parseReply(const std::string& text,
                                          QualityScore& out) const {
    return parseJsonScores(text, out);
}

QualityScore RequirementQualityService::heuristicScore(
    const tracelink::Entity& req) const {
    std::string body = req.text.empty() ? req.name : req.text;
    std::string text = trim(body);
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

    // Atomicity: single "shall" (single responsibility), short scope, no
    // conjunction splitting into multiple requirements.
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

QualityScore RequirementQualityService::scoreRequirement(
    const tracelink::Entity& req) const {
    std::string model = cfg_.param("model", "qwen2.5:7b");
    std::string prompt = buildPrompt(req);

    try {
        lodestar::Json params = lodestar::Json::object();
        params["model"] = lodestar::Json::string(model);
        params["prompt"] = lodestar::Json::string(prompt);
        lodestar::Json reply = llm_.invoke("complete", params);

        std::string text;
        if (reply.isObject() && reply.has("reply")) {
            text = extractReplyText(reply.at("reply"));
        } else {
            text = extractReplyText(reply);
        }

        QualityScore s;
        if (!text.empty() && parseReply(text, s)) {
            // Recompute overall from the parsed dimensions if it was missing.
            if (s.overall == 0) {
                s.overall = clamp100((s.clarity + s.testability + s.atomicity +
                                      s.completeness + s.ambiguity) / 5);
            }
            return s;
        }
        // Unparseable reply -> deterministic fallback.
        return heuristicScore(req);
    } catch (const adapters::AdapterError&) {
        // No live server / network / timeout -> deterministic fallback.
        return heuristicScore(req);
    } catch (const std::exception&) {
        // Any other failure -> deterministic fallback.
        return heuristicScore(req);
    }
}

}  // namespace lodestar::riskai
