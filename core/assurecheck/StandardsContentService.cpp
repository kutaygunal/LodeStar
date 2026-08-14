// core/assurecheck/StandardsContentService.cpp
// Gap-Fill AssureCheck 2.2: vetted standards-content library.

#include "core/assurecheck/StandardsContentService.h"

#include <fstream>
#include <sstream>

#include "core/adapters/Json.h"

namespace lodestar::assurecheck {

namespace {

std::string readFile(const std::string& path, bool& ok) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { ok = false; return ""; }
    std::ostringstream ss;
    ss << in.rdbuf();
    ok = true;
    return ss.str();
}

bool validDalRange(const std::string& dal) {
    // A-D, A-C, A-B, A, A-E, B-D, etc. Non-empty, all in [A-E].
    if (dal.empty()) return false;
    for (char c : dal) {
        if (c == '-') continue;
        if (c < 'A' || c > 'E') return false;
    }
    return true;
}

bool validDal(const std::string& dal) {
    return dal.size() == 1 && dal[0] >= 'A' && dal[0] <= 'E';
}

}  // namespace

common::Result<void> StandardsContentService::validateBundle(
    const StandardBundle& bundle) const {
    if (bundle.schemaVersion.empty())
        return common::Result<void>::err("bundle has no schemaVersion");
    if (bundle.bundleVersion.empty())
        return common::Result<void>::err("bundle has no bundleVersion");
    if (bundle.standardCode.empty())
        return common::Result<void>::err("bundle has no standardCode");
    if (bundle.objectives.empty())
        return common::Result<void>::err("bundle has no objectives");

    for (const auto& obj : bundle.objectives) {
        if (obj.code.empty())
            return common::Result<void>::err("objective has no code");
        if (!validDalRange(obj.dalApplicability))
            return common::Result<void>::err(
                "objective " + obj.code + " has invalid dalApplicability");
        if (obj.subObjectives.empty())
            return common::Result<void>::err(
                "objective " + obj.code + " has no sub-objectives");
        for (const auto& sub : obj.subObjectives) {
            if (sub.code.empty())
                return common::Result<void>::err(
                    "objective " + obj.code + " has a sub-objective without code");
            if (sub.objective.empty())
                return common::Result<void>::err(
                    "sub-objective " + sub.code + " has no objective text");
            if (!validDalRange(sub.dal))
                return common::Result<void>::err(
                    "sub-objective " + sub.code + " has invalid dal range");
        }
    }
    return common::Result<void>::ok();
}

common::Result<StandardBundle> StandardsContentService::loadBundle(
    const std::string& path) const {
    bool ok = false;
    std::string text = readFile(path, ok);
    if (!ok) {
        return common::Result<StandardBundle>::err(
            common::ErrorCode::IoError, "cannot read bundle file: " + path);
    }

    lodestar::Json root;
    try {
        root = lodestar::Json::parse(text);
    } catch (...) {
        return common::Result<StandardBundle>::err(
            common::ErrorCode::IoError, "bundle is not valid JSON: " + path);
    }
    if (!root.isObject()) {
        return common::Result<StandardBundle>::err(
            common::ErrorCode::IoError, "bundle root must be an object");
    }

    StandardBundle bundle;
    if (root.has("schemaVersion")) bundle.schemaVersion = root.at("schemaVersion").asString();
    if (root.has("bundleVersion")) bundle.bundleVersion = root.at("bundleVersion").asString();
    if (root.has("standard")) bundle.standardCode = root.at("standard").asString();
    if (root.has("name")) bundle.name = root.at("name").asString();

    if (root.has("objectives") && root.at("objectives").isArray()) {
        for (const auto& o : root.at("objectives").asArray()) {
            if (!o.isObject()) continue;
            StandardObjective obj;
            if (o.has("code")) obj.code = o.at("code").asString();
            if (o.has("title")) obj.title = o.at("title").asString();
            if (o.has("dalApplicability"))
                obj.dalApplicability = o.at("dalApplicability").asString();
            if (o.has("subObjectives") && o.at("subObjectives").isArray()) {
                for (const auto& s : o.at("subObjectives").asArray()) {
                    if (!s.isObject()) continue;
                    StandardSubObjective sub;
                    if (s.has("code")) sub.code = s.at("code").asString();
                    if (s.has("objective")) sub.objective = s.at("objective").asString();
                    if (s.has("dal")) sub.dal = s.at("dal").asString();
                    if (s.has("evidence")) sub.evidence = s.at("evidence").asString();
                    obj.subObjectives.push_back(std::move(sub));
                }
            }
            bundle.objectives.push_back(std::move(obj));
        }
    }

    auto v = validateBundle(bundle);
    if (v.failed()) {
        return common::Result<StandardBundle>::err(
            common::ErrorCode::ValidationFailed, v.error());
    }
    return common::Result<StandardBundle>::ok(std::move(bundle));
}

bool StandardsContentService::appliesToDal(const std::string& range,
                                           const std::string& projectDal) {
    if (range.empty() || projectDal.size() != 1) return false;
    char p = projectDal[0];
    if (p < 'A' || p > 'E') return false;
    // Parse "A-D" -> lower A, upper D. A lone "A" applies only to A.
    char lo = range[0];
    char hi = lo;
    auto dash = range.find('-');
    if (dash != std::string::npos && dash + 1 < range.size()) {
        hi = range[dash + 1];
    }
    if (lo < 'A' || lo > 'E' || hi < lo || hi > 'E') return false;
    return p >= lo && p <= hi;
}

std::vector<StandardSubObjective> StandardsContentService::flatten(
    const StandardBundle& bundle) {
    std::vector<StandardSubObjective> out;
    for (const auto& obj : bundle.objectives) {
        for (const auto& sub : obj.subObjectives) {
            out.push_back(sub);
        }
    }
    return out;
}

}  // namespace lodestar::assurecheck
