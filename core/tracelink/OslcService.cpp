// core/tracelink/OslcService.cpp
// OSLC provider + consumer for the TraceLink model (S2 Phase 12).

#include "core/tracelink/OslcService.h"

#include <cctype>
#include <string>

namespace lodestar::tracelink {

namespace {

// XML-escape a text value for embedding in an RDF/XML element body.
std::string xmlEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c; break;
        }
    }
    return out;
}

// Trim leading/trailing ASCII whitespace.
std::string trim(const std::string& in) {
    size_t b = 0;
    size_t e = in.size();
    while (b < e && std::isspace(static_cast<unsigned char>(in[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(in[e - 1]))) --e;
    return in.substr(b, e - b);
}

// Extract the text content of the first occurrence of <tag>...</tag> in `xml`.
// Returns false if the tag is not present. Handles both `<tag>` and
// `<prefix:tag>` forms by matching on the local name suffix.
bool extractTag(const std::string& xml, const std::string& tag,
                std::string& out) {
    // Match an opening tag whose local name equals `tag` (e.g. "identifier").
    // We search for ":" + tag + ">" or "<" + tag + ">" to avoid matching
    // prefixes like "dcterms:identifier" partially.
    const std::string open1 = "<" + tag + ">";
    const std::string open2 = ":" + tag + ">";
    size_t pos = std::string::npos;
    size_t p1 = xml.find(open1);
    size_t p2 = xml.find(open2);
    if (p1 != std::string::npos && (p2 == std::string::npos || p1 < p2)) {
        pos = p1;
    } else if (p2 != std::string::npos) {
        pos = p2;
    }
    if (pos == std::string::npos) return false;

    size_t contentStart = pos + (p1 == pos ? open1.size() : open2.size());

    // Find the matching closing tag. It may be "</tag>" or "</prefix:tag>".
    // Walk forward from the content start looking for a "</" whose local
    // name (the text after the last ':' up to the '>') equals `tag`.
    size_t searchFrom = contentStart;
    while (true) {
        size_t closeOpen = xml.find("</", searchFrom);
        if (closeOpen == std::string::npos) return false;
        size_t gt = xml.find('>', closeOpen);
        if (gt == std::string::npos) return false;
        size_t nameStart = closeOpen + 2;
        size_t colon = xml.rfind(':', gt);
        if (colon != std::string::npos && colon > nameStart) {
            nameStart = colon + 1;
        }
        std::string localName = xml.substr(nameStart, gt - nameStart);
        if (localName == tag) {
            out = trim(xml.substr(contentStart, closeOpen - contentStart));
            return true;
        }
        searchFrom = gt + 1;
    }
}

}  // namespace

OslcService::OslcService(TraceLinkService& svc) : svc_(svc) {}

common::Result<std::string> OslcService::exportRequirementAsOslc(
    const std::string& requirementId) {
    auto got = svc_.getEntity(EntityType::Requirement, requirementId);
    if (got.failed()) {
        return common::Result<std::string>::err(got.errorCode(), got.error());
    }
    if (!got.value().has_value()) {
        return common::Result<std::string>::err(
            common::ErrorCode::NotFound,
            "requirement '" + requirementId + "' not found");
    }

    const Entity& e = got.value().value();
    std::string xml;
    xml += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml += "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"\n";
    xml += "         xmlns:dcterms=\"http://purl.org/dc/terms/\"\n";
    xml += "         xmlns:oslc_rm=\"http://open-services.net/ns/rm#\">\n";
    xml += "  <oslc_rm:Requirement rdf:about=\"";
    xml += xmlEscape(e.externalId);
    xml += "\">\n";
    xml += "    <dcterms:identifier>";
    xml += xmlEscape(e.externalId);
    xml += "</dcterms:identifier>\n";
    xml += "    <dcterms:title>";
    xml += xmlEscape(e.name);
    xml += "</dcterms:title>\n";
    xml += "  </oslc_rm:Requirement>\n";
    xml += "</rdf:RDF>\n";
    return common::Result<std::string>::ok(std::move(xml));
}

common::Result<Entity> OslcService::importRequirementFromOslc(
    const std::string& oslcResource) {
    std::string identifier;
    std::string title;
    if (!extractTag(oslcResource, "identifier", identifier)) {
        return common::Result<Entity>::err(
            common::ErrorCode::ValidationFailed,
            "OSLC resource missing dcterms:identifier");
    }
    if (!extractTag(oslcResource, "title", title)) {
        return common::Result<Entity>::err(
            common::ErrorCode::ValidationFailed,
            "OSLC resource missing dcterms:title");
    }
    if (identifier.empty() || title.empty()) {
        return common::Result<Entity>::err(
            common::ErrorCode::ValidationFailed,
            "OSLC resource has empty identifier or title");
    }

    Entity e;
    e.type = EntityType::Requirement;
    e.externalId = identifier;
    e.name = title;
    e.text = title;
    e.status = "Draft";

    // Create-or-update: if a requirement with the same external id already
    // exists in the model, update it (preserving its internal id and other
    // fields); otherwise create a new requirement. This makes the consumer
    // idempotent and supports round-trip export -> import.
    auto existing = svc_.listEntities(EntityType::Requirement, EntityFilter{});
    if (existing.isOk()) {
        for (const auto& ent : existing.value()) {
            if (ent.externalId == identifier) {
                Entity upd = ent;
                upd.name = title;
                upd.text = title;
                return svc_.updateEntity(upd);
            }
        }
    }
    return svc_.addEntity(e);
}

}  // namespace lodestar::tracelink
