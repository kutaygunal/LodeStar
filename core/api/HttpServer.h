// core/api/HttpServer.h
// Minimal embedded HTTP/1.1 server (Phase 5, P5-2.1).
//
// HTTP server decision: a small, self-contained winsock/BSD listener is used
// instead of vendoring cpp-httplib. This keeps the API layer dependency-free and
// satisfies the plan's "otherwise implement a minimal listener" option. Supports
// method + path routing with `<name>` path parameters, request body parsing, and
// HTTP status + JSON responses.

#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace lodestar::api {

struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> params;  // path <params> + query
    std::string body;
};

struct HttpResponse {
    int status = 200;
    std::string contentType = "application/json";
    std::string body;
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

class HttpServer {
public:
    HttpServer() = default;
    ~HttpServer();
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    // Register a handler for a method + path pattern. Path patterns use '<name>'
    // for dynamic segments, e.g. "/adapters/<name>/invoke". An empty method
    // matches any method (catch-all for a path).
    void route(const std::string& method, const std::string& pattern,
               HttpHandler handler);

    // Start listening on the given port. Returns false if the socket cannot be
    // created/bound. Accepts requests on a background thread until stop().
    bool start(int port);
    void stop();

    // The port actually bound (useful for port 0 / ephemeral allocation).
    int port() const { return boundPort_; }

private:
    void acceptLoop();
    void handleClient(int fd);
    HttpResponse dispatch(const HttpRequest& req) const;

    struct Route {
        std::string method;
        std::string pattern;
        std::vector<std::string> segs;
        HttpHandler handler;
    };

    std::vector<Route> routes_;
    int listenFd_ = -1;
    std::atomic<int> boundPort_{0};
    std::atomic<bool> running_{false};
    std::thread acceptThread_;
};

}  // namespace lodestar::api
