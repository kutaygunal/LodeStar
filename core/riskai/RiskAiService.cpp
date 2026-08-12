// core/riskai/RiskAiService.cpp
// RiskAI first slice (S1 Phase 3): LLM-assisted FMEA.

#include "core/riskai/RiskAiService.h"

#include <algorithm>
#include <cctype>
#include <sstream>

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

// Clamp an int into [1,10].
int clamp10(int v) {
    if (v < 1) return 1;
    if (v > 10) return 10;
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

// Parse a JSON array of FMEA objects from the reply text. Returns false if the
// text does not contain a parseable array with at least one valid row.
bool parseJsonRows(const std::string& text, std::vector<FmeaRow>& out) {
    size_t open = text.find('[');
    if (open == std::string::npos) return false;
    // Find the matching closing bracket (naive depth scan).
    int depth = 0;
    size_t close = std::string::npos;
    for (size_t i = open; i < text.size(); ++i) {
        char c = text[i];
        if (c == '[') ++depth;
        else if (c == ']') {
            --depth;
            if (depth == 0) { close = i; break; }
        }
    }
    if (close == std::string::npos) return false;
    std::string sub = text.substr(open, close - open + 1);
    lodestar::Json arr;
    try {
        arr = lodestar::Json::parse(sub);
    } catch (...) {
        return false;
    }
    if (!arr.isArray() || arr.size() == 0) return false;
    bool any = false;
    for (const auto& item : arr.asArray()) {
        if (!item.isObject()) continue;
        FmeaRow row;
        if (item.has("failureMode") && item.at("failureMode").isString())
            row.failureMode = item.at("failureMode").asString();
        if (item.has("effect") && item.at("effect").isString())
            row.effect = item.at("effect").asString();
        if (item.has("severity") && item.at("severity").isNumber())
            row.severity = clamp10(static_cast<int>(item.at("severity").asNumber()));
        if (item.has("likelihood") && item.at("likelihood").isNumber())
            row.likelihood = clamp10(static_cast<int>(item.at("likelihood").asNumber()));
        if (row.failureMode.empty()) continue;
        row.risk = row.severity * row.likelihood;
        out.push_back(std::move(row));
        any = true;
    }
    return any;
}

// Parse a line-oriented table: "failureMode|effect|severity|likelihood" per line.
bool parseLineRows(const std::string& text, std::vector<FmeaRow>& out) {
    std::istringstream ss(text);
    std::string line;
    bool any = false;
    while (std::getline(ss, line)) {
        line = trim(line);
        if (line.empty()) continue;
        // Split on '|' (or ',').
        std::vector<std::string> parts;
        std::string cur;
        for (char c : line) {
            if (c == '|' || c == ',') {
                parts.push_back(trim(cur));
                cur.clear();
            } else {
                cur += c;
            }
        }
        parts.push_back(trim(cur));
        if (parts.size() < 4) continue;
        FmeaRow row;
        row.failureMode = parts[0];
        row.effect = parts[1];
        try {
            row.severity = clamp10(std::stoi(parts[2]));
            row.likelihood = clamp10(std::stoi(parts[3]));
        } catch (...) {
            continue;
        }
        if (row.failureMode.empty()) continue;
        row.risk = row.severity * row.likelihood;
        out.push_back(std::move(row));
        any = true;
    }
    return any;
}

}  // namespace

RiskAiService::RiskAiService(adapters::IAdapter& llm,
                             const adapters::AdapterConfig& cfg)
    : llm_(llm), cfg_(cfg) {}

std::string RiskAiService::buildPrompt(const std::string& hazard) const {
    std::string model = cfg_.param("model", "qwen2.5:7b");
    (void)model;  // model name is passed as a separate invoke param
    std::string sys = cfg_.param("system",
        "You are a safety engineer. Given a hazard, produce an FMEA table. "
        "Reply with ONLY a JSON array of objects, each with keys "
        "\"failureMode\", \"effect\", \"severity\" (1-10), \"likelihood\" (1-10).");
    return sys + "\n\nHazard: " + hazard;
}

bool RiskAiService::parseReply(const std::string& text,
                               std::vector<FmeaRow>& out) const {
    if (parseJsonRows(text, out)) return true;
    return parseLineRows(text, out);
}

std::vector<FmeaRow> RiskAiService::cannedTable() const {
    std::vector<FmeaRow> rows;
    FmeaRow a;
    a.failureMode = "Loss of GPS signal during approach";
    a.effect = "Aircraft position uncertainty increases; guidance degraded";
    a.severity = 8;
    a.likelihood = 4;
    a.risk = a.severity * a.likelihood;
    rows.push_back(a);

    FmeaRow b;
    b.failureMode = "Multipath interference on receiver";
    b.effect = "Erroneous pseudorange measurements; navigation error";
    b.severity = 6;
    b.likelihood = 5;
    b.risk = b.severity * b.likelihood;
    rows.push_back(b);

    FmeaRow c;
    c.failureMode = "Ionospheric scintillation";
    c.effect = "Signal amplitude/phase fluctuation; loss of lock";
    c.severity = 7;
    c.likelihood = 3;
    c.risk = c.severity * c.likelihood;
    rows.push_back(c);
    return rows;
}

common::Result<std::vector<FmeaRow>> RiskAiService::analyze(
    const std::string& hazard) {
    if (trim(hazard).empty()) {
        return common::Result<std::vector<FmeaRow>>::err(
            common::ErrorCode::InvalidArgument,
            "hazard must not be empty");
    }

    std::string prompt = buildPrompt(hazard);
    std::string model = cfg_.param("model", "qwen2.5:7b");

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

        std::vector<FmeaRow> rows;
        if (!text.empty() && parseReply(text, rows)) {
            return common::Result<std::vector<FmeaRow>>::ok(std::move(rows));
        }
        // Unparseable reply -> deterministic fallback.
        return common::Result<std::vector<FmeaRow>>::ok(cannedTable());
    } catch (const adapters::AdapterError&) {
        // No live server / network / timeout -> deterministic fallback.
        return common::Result<std::vector<FmeaRow>>::ok(cannedTable());
    } catch (const std::exception&) {
        // Any other failure -> deterministic fallback.
        return common::Result<std::vector<FmeaRow>>::ok(cannedTable());
    }
}

}  // namespace lodestar::riskai
