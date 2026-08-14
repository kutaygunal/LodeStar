// core/riskai/RequirementQualityService.cpp
// S2 Phase 13: AI quality scoring on requirements.

#include "core/riskai/RequirementQualityService.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

#include "core/common/QualityScoring.h"
namespace lodestar::riskai {

namespace {

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
    // Gap-Fill 1.7: delegate to the shared five-dimension scoring so RiskAI and
    // TraceLink agree exactly. The shared service is deterministic and lives in
    // core/common (no LLM, no dependency cycle).
    lodestar::common::QualityScore shared =
        lodestar::common::scoreQuality(body);
    QualityScore s;
    s.clarity = shared.clarity;
    s.testability = shared.testability;
    s.atomicity = shared.atomicity;
    s.completeness = shared.completeness;
    s.ambiguity = shared.ambiguity;
    s.overall = shared.overall;
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
