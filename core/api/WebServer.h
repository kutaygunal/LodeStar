#pragma once
// core/api/WebServer.h
// S2 Phase 2: web / browser read-review layer over the REST API.
//
// Serves a lightweight browser UI over the existing services:
//   GET /web/             -> HTML landing page
//   GET /web/requirements -> requirements as HTML (review)
//   GET /web/trace        -> trace matrix as HTML
//   GET /web/assure       -> AssureCheck compliance summary as HTML
//
// Auth-aware (S2 Phase 1): honors login/roles. A valid session token (Bearer)
// is validated; a viewer can read, an editor can review. When no token is
// supplied the layer serves public read-only content so the review UI is
// immediately usable; an invalid token is rejected with 401.

#include <string>

#include "core/api/HttpServer.h"
#include "core/assurecheck/AssureCheckService.h"
#include "core/assurecheck/ComplianceEngine.h"
#include "core/persistence/Database.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/UserService.h"

namespace lodestar::api {

class WebServer {
public:
    explicit WebServer(persistence::Database& db);

    // Registers every /web route on the given server.
    void setup(HttpServer& server);

private:
    HttpResponse webRoot(const HttpRequest& req);
    HttpResponse webRequirements(const HttpRequest& req);
    HttpResponse webTrace(const HttpRequest& req);
    HttpResponse webAssure(const HttpRequest& req);

    // Auth helpers.
    std::string currentRole(const HttpRequest& req);
    bool canRead(const HttpRequest& req);
    HttpResponse unauthorized() const;

    persistence::Database& db_;
    tracelink::TraceLinkService tracelink_;
    assurecheck::AssureCheckService assure_;
    assurecheck::ComplianceEngine compliance_;
    tracelink::UserService users_;
};

}  // namespace lodestar::api
