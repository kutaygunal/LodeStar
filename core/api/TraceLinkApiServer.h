// core/api/TraceLinkApiServer.h
// WP-6 REST exposure of the TraceLink service layer. Registers every
// /tracelink route on the embedded HttpServer, converting service-layer
// results (entities, links, coverage, matrix, rules, baselines, import/export)
// into the standard HTTP JSON contract.
//
// Contract written by the scrum-master in core/test/wp6_api_tests.cpp:
//   - Success: 200 with the documented JSON shape.
//   - Error model for 400/404/500:
//         {"error":{"code":<int>,"message":"..."}}
//
// Route table (docs/tracelink-plan.md, WP-6 / section 5):
//   GET    /tracelink/entities?type=&filter=
//   POST   /tracelink/entities
//   GET    /tracelink/entities/{type}/{id}
//   PUT    /tracelink/entities/{type}/{id}
//   DELETE /tracelink/entities/{type}/{id}
//   GET    /tracelink/entities/{type}/{id}/history
//   POST   /tracelink/links
//   GET    /tracelink/links?sourceType=&sourceId=
//   PUT    /tracelink/links/{id}
//   DELETE /tracelink/links/{id}
//   GET    /tracelink/impact/{type}/{id}
//   GET    /tracelink/coverage
//   GET    /tracelink/matrix
//   POST   /tracelink/validate
//   GET    /tracelink/rules
//   POST   /tracelink/baselines
//   GET    /tracelink/baselines
//   GET    /tracelink/baselines/{a}/diff?against={b}
//   POST   /tracelink/import/{format}     (csv | reqif)
//   GET    /tracelink/export/{format}     (csv | reqif | html)

#pragma once

#include <string>

#include "core/api/HttpServer.h"
#include "core/tracelink/BaselineService.h"
#include "core/tracelink/GraphEngine.h"
#include "core/tracelink/IoService.h"
#include "core/tracelink/RulesEngine.h"
#include "core/tracelink/TraceLinkService.h"

namespace lodestar::api {

class TraceLinkApiServer {
public:
    TraceLinkApiServer(tracelink::TraceLinkService& svc,
                       tracelink::GraphEngine& graph,
                       tracelink::RulesEngine& rules,
                       tracelink::BaselineService& baseline,
                       tracelink::IoService& io);

    // Register every /tracelink route on the server.
    void setup(HttpServer& server);

private:
    HttpResponse entitiesList(const HttpRequest& req);
    HttpResponse entityCreate(const HttpRequest& req);
    HttpResponse entityGet(const HttpRequest& req);
    HttpResponse entityUpdate(const HttpRequest& req);
    HttpResponse entityDelete(const HttpRequest& req);
    HttpResponse entityHistory(const HttpRequest& req);

    HttpResponse linkCreate(const HttpRequest& req);
    HttpResponse linkUpdate(const HttpRequest& req);
    HttpResponse linkDelete(const HttpRequest& req);
    HttpResponse linksList(const HttpRequest& req);

    HttpResponse impact(const HttpRequest& req);
    HttpResponse coverage(const HttpRequest& req);
    HttpResponse matrix(const HttpRequest& req);
    HttpResponse validate(const HttpRequest& req);
    HttpResponse rulesList(const HttpRequest& req);

    HttpResponse baselineCreate(const HttpRequest& req);
    HttpResponse baselineList(const HttpRequest& req);
    HttpResponse baselineDiff(const HttpRequest& req);

    HttpResponse importRoute(const HttpRequest& req);
    HttpResponse exportRoute(const HttpRequest& req);

    tracelink::TraceLinkService& svc_;
    tracelink::GraphEngine& graph_;
    tracelink::RulesEngine& rules_;
    tracelink::BaselineService& baseline_;
    tracelink::IoService& io_;
};

}  // namespace lodestar::api
