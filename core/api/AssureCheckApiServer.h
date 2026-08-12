#pragma once
// core/api/AssureCheckApiServer.h
// Phase 11 WP-6 (AssureCheck): REST exposure of the AssureCheck service layer.
// Registers every /assurecheck route on the embedded HttpServer, converting
// service-layer results (standards, checks, summary, dashboard) into the
// standard HTTP JSON contract.
//
// Contract written by the scrum-master in core/test/wp6_assurecheck_tests.cpp:
//   - Success: 200 with the documented JSON shape.
//   - Error model for 400/404/500:
//         {"error":{"code":<int>,"message":"..."}}
//
// Route table (docs/wp6-assurecheck-task.md):
//   GET    /assurecheck/standards
//   GET    /assurecheck/standards/{code}
//   POST   /assurecheck/checks
//   GET    /assurecheck/summary?standard=<code>
//   GET    /assurecheck/dashboard

#pragma once

#include <string>

#include "core/api/HttpServer.h"
#include "core/assurecheck/AssureCheckService.h"
#include "core/assurecheck/ComplianceEngine.h"
#include "core/assurecheck/DashboardService.h"
#include "core/assurecheck/ReportService.h"

namespace lodestar::api {

class AssureCheckApiServer {
public:
    AssureCheckApiServer(assurecheck::AssureCheckService& standards,
                         assurecheck::ComplianceEngine& engine,
                         assurecheck::ReportService& reports,
                         assurecheck::DashboardService& dashboard);

    // Register every /assurecheck route on the server.
    void setup(HttpServer& server);

private:
    HttpResponse standardsList(const HttpRequest& req);
    HttpResponse standardGet(const HttpRequest& req);
    HttpResponse checksRun(const HttpRequest& req);
    HttpResponse summaryGet(const HttpRequest& req);
    HttpResponse dashboardGet(const HttpRequest& req);

    assurecheck::AssureCheckService& standards_;
    assurecheck::ComplianceEngine& engine_;
    assurecheck::ReportService& reports_;
    assurecheck::DashboardService& dashboard_;
};

}  // namespace lodestar::api
