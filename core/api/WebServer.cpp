// core/api/WebServer.cpp
// S2 Phase 2: web / browser read-review layer over the REST API.
//
// Serves a lightweight browser UI over the existing services. Reuses the
// embedded HttpServer and honors the S2 Phase 1 auth (login/roles): a viewer
// can read, an editor can review. When no token is supplied the layer serves
// public read-only content; an invalid token is rejected with 401.

#include "core/api/WebServer.h"

#include <string>
#include <vector>

#include "core/tracelink/Types.h"

namespace lodestar::api {

namespace tl = lodestar::tracelink;
namespace ac = lodestar::assurecheck;
namespace ra = lodestar::riskai;
namespace ih = lodestar::integratehub;

namespace {

// Minimal HTML escaping so seeded text cannot break the rendered page.
std::string htmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string pageHeader(const std::string& title) {
    return "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">"
           "<title>" + htmlEscape(title) + "</title>"
           "<style>body{font-family:sans-serif;margin:2em;}"
           "table{border-collapse:collapse;margin-top:1em;}"
           "th,td{border:1px solid #ccc;padding:6px 10px;text-align:left;}"
           "th{background:#f0f0f0;}a{margin-right:1em;}</style>"
           "</head><body>";
}

std::string pageFooter() { return "</body></html>"; }

// Extracts a session token from the "Authorization: Bearer <token>" header or
// the "token" query parameter. Returns "" if absent.
std::string bearerToken(const HttpRequest& req) {
    auto it = req.headers.find("authorization");
    if (it != req.headers.end()) {
        const std::string& h = it->second;
        // Case-insensitive "Bearer " prefix.
        std::string lower;
        for (char c : h) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
            lower += c;
        }
        const std::string prefix = "bearer ";
        if (lower.size() > prefix.size() &&
            lower.compare(0, prefix.size(), prefix) == 0) {
            return h.substr(prefix.size());
        }
    }
    auto q = req.params.find("token");
    if (q != req.params.end()) return q->second;
    return "";
}

}  // namespace

WebServer::WebServer(persistence::Database& db)
    : db_(db), tracelink_(db), assure_(db), compliance_(db), users_(db) {}

void WebServer::setup(HttpServer& server) {
    server.route("GET", "/web/",
                 [this](const HttpRequest& r) { return webRoot(r); });
    server.route("GET", "/web/requirements",
                 [this](const HttpRequest& r) { return webRequirements(r); });
    server.route("GET", "/web/trace",
                 [this](const HttpRequest& r) { return webTrace(r); });
    server.route("GET", "/web/assure",
                 [this](const HttpRequest& r) { return webAssure(r); });
    server.route("GET", "/web/riskai",
                 [this](const HttpRequest& r) { return webRiskai(r); });
    server.route("GET", "/web/integratehub",
                 [this](const HttpRequest& r) { return webIntegratehub(r); });
}

// ---------------------------------------------------------------------------
// Auth helpers.
// ---------------------------------------------------------------------------
std::string WebServer::currentRole(const HttpRequest& req) {
    std::string token = bearerToken(req);
    if (token.empty()) return "";
    auto res = users_.currentUser(token);
    if (res.failed()) return "";
    return res.value().role;
}

bool WebServer::canRead(const HttpRequest& req) {
    std::string token = bearerToken(req);
    if (token.empty()) return true;  // public read-only review layer
    auto res = users_.currentUser(token);
    return res.isOk();  // any valid session (viewer/editor/admin) can read
}

HttpResponse WebServer::unauthorized() const {
    HttpResponse r;
    r.status = 401;
    r.contentType = "application/json";
    r.body = "{\"error\":{\"code\":401,\"message\":\"unauthorized\"}}";
    return r;
}

// ---------------------------------------------------------------------------
// GET /web/  -> HTML landing page.
// ---------------------------------------------------------------------------
HttpResponse WebServer::webRoot(const HttpRequest& req) {
    if (!canRead(req)) return unauthorized();
    std::string role = currentRole(req);
    std::string html = pageHeader("Lodestar — Read / Review");
    html += "<h1>Lodestar — Read / Review</h1>";
    html += "<p>Role: " +
            htmlEscape(role.empty() ? "public (read-only)" : role) + "</p>";
    html += "<ul>";
    html += "<li><a href=\"/web/requirements\">Requirements</a></li>";
    html += "<li><a href=\"/web/trace\">Trace Matrix</a></li>";
    html += "<li><a href=\"/web/assure\">AssureCheck Compliance</a></li>";
    html += "<li><a href=\"/web/riskai\">RiskAI FMEA</a></li>";
    html += "<li><a href=\"/web/integratehub\">IntegrateHub</a></li>";
    html += "</ul>";
    html += pageFooter();
    HttpResponse r;
    r.contentType = "text/html";
    r.body = html;
    return r;
}

// ---------------------------------------------------------------------------
// GET /web/requirements -> requirements as HTML for review.
// ---------------------------------------------------------------------------
HttpResponse WebServer::webRequirements(const HttpRequest& req) {
    if (!canRead(req)) return unauthorized();
    std::string html = pageHeader("Lodestar — Requirements");
    html += "<h1>Requirements</h1>";
    html += "<table><tr><th>External ID</th><th>Name</th><th>Status</th>"
            "<th>Version</th></tr>";
    auto res = tracelink_.listEntities(tl::EntityType::Requirement,
                                       tl::EntityFilter{});
    if (res.isOk()) {
        for (const auto& e : res.value()) {
            html += "<tr><td>" + htmlEscape(e.externalId) + "</td><td>" +
                    htmlEscape(e.name) + "</td><td>" + htmlEscape(e.status) +
                    "</td><td>" + std::to_string(e.version) + "</td></tr>";
        }
    }
    html += "</table>";
    html += "<p><a href=\"/web/\">Back</a></p>";
    html += pageFooter();
    HttpResponse r;
    r.contentType = "text/html";
    r.body = html;
    return r;
}

// ---------------------------------------------------------------------------
// GET /web/trace -> trace matrix as HTML.
// ---------------------------------------------------------------------------
HttpResponse WebServer::webTrace(const HttpRequest& req) {
    if (!canRead(req)) return unauthorized();
    std::string html = pageHeader("Lodestar — Trace Matrix");
    html += "<h1>Trace Matrix</h1>";

    auto reqs = tracelink_.listEntities(tl::EntityType::Requirement,
                                        tl::EntityFilter{});
    auto designs = tracelink_.listEntities(tl::EntityType::Design,
                                           tl::EntityFilter{});
    auto links = tracelink_.allLinks();

    // Build a lookup: "sourceId|targetId" -> relation, for requirement->design.
    std::vector<std::string> reqIds;
    std::vector<std::string> designIds;
    if (reqs.isOk())
        for (const auto& e : reqs.value()) reqIds.push_back(e.id);
    if (designs.isOk())
        for (const auto& e : designs.value()) designIds.push_back(e.id);

    // Matrix table: rows = requirements, columns = design items.
    html += "<table><tr><th>Requirement</th>";
    for (const auto& d : designIds) {
        html += "<th>" + htmlEscape(d) + "</th>";
    }
    html += "</tr>";
    for (const auto& r : reqIds) {
        html += "<tr><td>" + htmlEscape(r) + "</td>";
        for (const auto& d : designIds) {
            std::string cell = "&mdash;";
            if (links.isOk()) {
                for (const auto& l : links.value()) {
                    if (l.sourceType == tl::EntityType::Requirement &&
                        l.sourceId == r && l.targetType == tl::EntityType::Design &&
                        l.targetId == d) {
                        cell = htmlEscape(l.relation);
                        break;
                    }
                }
            }
            html += "<td>" + cell + "</td>";
        }
        html += "</tr>";
    }
    html += "</table>";

    // Link detail list (the trace link data).
    html += "<h2>Trace Links</h2><ul>";
    if (links.isOk()) {
        for (const auto& l : links.value()) {
            html += "<li>" + htmlEscape(tl::toString(l.sourceType)) + " " +
                    htmlEscape(l.sourceId) + " &rarr; " +
                    htmlEscape(tl::toString(l.targetType)) + " " +
                    htmlEscape(l.targetId) + " [" + htmlEscape(l.relation) +
                    "]</li>";
        }
    }
    html += "</ul>";
    html += "<p><a href=\"/web/\">Back</a></p>";
    html += pageFooter();

    HttpResponse r;
    r.contentType = "text/html";
    r.body = html;
    return r;
}

// ---------------------------------------------------------------------------
// GET /web/assure -> AssureCheck compliance summary as HTML.
// ---------------------------------------------------------------------------
HttpResponse WebServer::webAssure(const HttpRequest& req) {
    if (!canRead(req)) return unauthorized();
    std::string html = pageHeader("Lodestar — AssureCheck Compliance");
    html += "<h1>AssureCheck Compliance Summary</h1>";

    // Ensure the assurance standards are seeded (idempotent).
    assure_.seedStandards();

    // Evaluate DO-178C at DAL A against the current project data and persist.
    auto run = compliance_.runChecks("DO-178C", "A");
    if (run.isOk()) compliance_.storeResults(run.value());

    auto sum = compliance_.summaryFor("DO-178C");
    if (sum.isOk()) {
        const ac::CheckSummary& s = sum.value();
        html += "<table><tr><th>Total</th><th>Pass</th><th>Fail</th>"
                "<th>NA</th><th>Warning</th><th>Percent</th></tr>";
        html += "<tr><td>" + std::to_string(s.total) + "</td><td>" +
                std::to_string(s.pass) + "</td><td>" + std::to_string(s.fail) +
                "</td><td>" + std::to_string(s.na) + "</td><td>" +
                std::to_string(s.warning) + "</td><td>" +
                std::to_string(s.percent) + "%</td></tr></table>";
        html += "<p>Pass count: " + std::to_string(s.pass) +
                " of " + std::to_string(s.total) + ".</p>";
    } else {
        html += "<p>Compliance summary unavailable: " +
                htmlEscape(sum.error()) + "</p>";
    }
    html += "<p><a href=\"/web/\">Back</a></p>";
    html += pageFooter();

    HttpResponse r;
    r.contentType = "text/html";
    r.body = html;
    return r;
}

// ---------------------------------------------------------------------------
// GET /web/riskai -> RiskAI FMEA workflows as HTML (review layer).
// ---------------------------------------------------------------------------
HttpResponse WebServer::webRiskai(const HttpRequest& req) {
    if (!canRead(req)) return unauthorized();
    std::string html = pageHeader("Lodestar — RiskAI FMEA");
    html += "<h1>RiskAI FMEA Workflows</h1>";

    ra::FmeaWorkflowService fmea(db_);
    auto wfs = fmea.listWorkflows();
    if (wfs.isOk()) {
        html += "<table><tr><th>Name</th><th>System</th><th>Stage</th>"
                "<th>Created</th></tr>";
        for (const auto& w : wfs.value()) {
            html += "<tr><td>" + htmlEscape(w.name) + "</td><td>" +
                    htmlEscape(w.system) + "</td><td>" +
                    htmlEscape(ra::stageName(w.stage)) + "</td><td>" +
                    htmlEscape(w.createdAt) + "</td></tr>";
        }
        html += "</table>";
    } else {
        html += "<p>No FMEA workflows: " + htmlEscape(wfs.error()) + "</p>";
    }
    html += "<p><a href=\"/web/\">Back</a></p>";
    html += pageFooter();

    HttpResponse r;
    r.contentType = "text/html";
    r.body = html;
    return r;
}

// ---------------------------------------------------------------------------
// GET /web/integratehub -> IntegrateHub PR/CR as HTML (review layer).
// ---------------------------------------------------------------------------
HttpResponse WebServer::webIntegratehub(const HttpRequest& req) {
    if (!canRead(req)) return unauthorized();
    std::string html = pageHeader("Lodestar — IntegrateHub");
    html += "<h1>IntegrateHub Problem Reports &amp; Change Requests</h1>";

    ih::ImpactAnalysisService impact(db_);
    auto prs = impact.listPrs();
    auto crs = impact.listCrs();

    html += "<h2>Problem Reports</h2><table><tr><th>Title</th><th>Severity</th>"
            "<th>Status</th></tr>";
    if (prs.isOk()) {
        for (const auto& p : prs.value()) {
            html += "<tr><td>" + htmlEscape(p.title) + "</td><td>" +
                    htmlEscape(p.severity) + "</td><td>" +
                    htmlEscape(p.status) + "</td></tr>";
        }
    }
    html += "</table>";

    html += "<h2>Change Requests</h2><table><tr><th>Title</th><th>Status</th>"
            "<th>Entity</th></tr>";
    if (crs.isOk()) {
        for (const auto& c : crs.value()) {
            html += "<tr><td>" + htmlEscape(c.title) + "</td><td>" +
                    htmlEscape(c.status) + "</td><td>" +
                    htmlEscape(c.entityType + ":" + c.entityId) +
                    "</td></tr>";
        }
    }
    html += "</table>";

    html += "<p><a href=\"/web/\">Back</a></p>";
    html += pageFooter();

    HttpResponse r;
    r.contentType = "text/html";
    r.body = html;
    return r;
}

}  // namespace lodestar::api
