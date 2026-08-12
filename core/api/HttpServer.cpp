// core/api/HttpServer.cpp
// Minimal embedded HTTP/1.1 server over winsock/BSD sockets.

#include "core/api/HttpServer.h"

#include <cstdio>
#include <cstring>
#include <chrono>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using SockLen_t = int;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SockLen_t = socklen_t;
#endif

namespace lodestar::api {

namespace {

void closeSocket(int fd) {
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
}

std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : path) {
        if (c == '/') {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

// Split a URI into path and query; decode simple percent escapes in query values.
std::string pathOnly(const std::string& uri) {
    size_t q = uri.find('?');
    return q == std::string::npos ? uri : uri.substr(0, q);
}

void parseQuery(const std::string& uri, std::map<std::string, std::string>& params) {
    size_t q = uri.find('?');
    if (q == std::string::npos) return;
    std::string query = uri.substr(q + 1);
    size_t start = 0;
    while (start <= query.size()) {
        size_t amp = query.find('&', start);
        std::string kv = query.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
        size_t eq = kv.find('=');
        if (eq != std::string::npos) {
            params[kv.substr(0, eq)] = kv.substr(eq + 1);
        } else if (!kv.empty()) {
            params[kv] = "";
        }
        if (amp == std::string::npos) break;
        start = amp + 1;
    }
}

// Parse the raw header block (everything up to the blank line) into a map of
// lowercased header name -> trimmed value. The first line is the request line
// and is skipped.
void parseHeaders(const std::string& block, std::map<std::string, std::string>& out) {
    size_t start = 0;
    while (start < block.size()) {
        size_t eol = block.find('\n', start);
        std::string line = block.substr(start, eol == std::string::npos ? std::string::npos : eol - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name;
            for (size_t i = 0; i < colon; ++i) {
                char c = line[i];
                if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
                name.push_back(c);
            }
            std::string value = line.substr(colon + 1);
            // Trim leading/trailing whitespace.
            size_t b = 0, e = value.size();
            while (b < e && (value[b] == ' ' || value[b] == '\t')) ++b;
            while (e > b && (value[e - 1] == ' ' || value[e - 1] == '\t')) --e;
            out[name] = value.substr(b, e - b);
        }
        if (eol == std::string::npos) break;
        start = eol + 1;
    }
}

}  // namespace

HttpServer::~HttpServer() { stop(); }

void HttpServer::route(const std::string& method, const std::string& pattern,
                       HttpHandler handler) {
    Route r;
    r.method = method;
    r.pattern = pattern;
    r.segs = splitPath(pattern);
    r.handler = std::move(handler);
    routes_.push_back(std::move(r));
}

bool HttpServer::start(int port) {
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
#endif

    listenFd_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (listenFd_ < 0) return false;

    int opt = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // bind loopback only (local API)
    addr.sin_port = htons(static_cast<unsigned short>(port));

    if (bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closeSocket(listenFd_);
        listenFd_ = -1;
        return false;
    }
    if (listen(listenFd_, 8) != 0) {
        closeSocket(listenFd_);
        listenFd_ = -1;
        return false;
    }

    sockaddr_in bound;
    SockLen_t blen = sizeof(bound);
    std::memset(&bound, 0, sizeof(bound));
    if (getsockname(listenFd_, reinterpret_cast<sockaddr*>(&bound), &blen) == 0) {
        boundPort_.store(static_cast<int>(ntohs(bound.sin_port)));
    } else {
        boundPort_.store(port);
    }

    running_.store(true);
    acceptThread_ = std::thread([this] { acceptLoop(); });
    return true;
}

void HttpServer::stop() {
    running_.store(false);
    if (listenFd_ >= 0) {
        closeSocket(listenFd_);
        listenFd_ = -1;
    }
    if (acceptThread_.joinable()) acceptThread_.join();
}

void HttpServer::acceptLoop() {
    while (running_.load()) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listenFd_, &rfds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200000;  // wake every 200 ms to check running_
        int sel = select(listenFd_ + 1, &rfds, nullptr, nullptr, &tv);
        if (sel < 0) {
            if (!running_.load()) break;
            continue;
        }
        if (sel == 0) continue;  // timeout: loop and re-check running_

        sockaddr_in client;
        SockLen_t len = sizeof(client);
        int fd = static_cast<int>(accept(listenFd_, reinterpret_cast<sockaddr*>(&client), &len));
        if (fd < 0) continue;
        std::fprintf(stderr, "[DBG] acceptLoop accepted fd=%d\n", fd); fflush(stderr);

        // Defensive recv timeout so a malformed client can never wedge the
        // single accept thread forever.
        struct timeval rtv;
        rtv.tv_sec = 10;
        rtv.tv_usec = 0;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&rtv), sizeof(rtv));
        try {
            handleClient(fd);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[HttpServer] request handler error: %s\n", e.what());
        } catch (...) {
            std::fprintf(stderr, "[HttpServer] request handler unknown error\n");
        }
    }
}

HttpResponse HttpServer::dispatch(const HttpRequest& req) const {
    std::vector<std::string> reqSegs = splitPath(pathOnly(req.path));
    for (const auto& r : routes_) {
        if (!r.method.empty() && r.method != req.method) continue;
        if (r.segs.size() != reqSegs.size()) continue;
        std::map<std::string, std::string> params = req.params;
        bool match = true;
        for (size_t i = 0; i < r.segs.size(); ++i) {
            const std::string& pat = r.segs[i];
            if (pat.size() >= 2 && pat.front() == '<' && pat.back() == '>') {
                params[pat.substr(1, pat.size() - 2)] = reqSegs[i];
            } else if (pat != reqSegs[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            HttpRequest clone = req;
            clone.params = std::move(params);
            return r.handler(clone);
        }
    }
    HttpResponse nf;
    nf.status = 404;
    nf.body = "{\"error\":{\"code\":404,\"message\":\"not found\"}}";
    return nf;
}

void HttpServer::handleClient(int fd) {
    auto t0 = std::chrono::steady_clock::now();
    std::fprintf(stderr, "[DBG] handleClient start fd=%d\n", fd); fflush(stderr);
    std::string raw;
    char buf[4096];
    bool headerDone = false;
    size_t contentLength = 0;
    size_t headerEnd = 0;

    while (true) {
        int n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        raw.append(buf, static_cast<size_t>(n));

        if (!headerDone) {
            size_t pos = raw.find("\r\n\r\n");
            bool crlf = pos != std::string::npos;
            if (!crlf) pos = raw.find("\n\n");
            if (pos != std::string::npos) {
                headerEnd = pos + (crlf ? 4 : 2);
                headerDone = true;
                std::string headers = raw.substr(0, headerEnd);
                // Content-Length
                std::string lower;
                for (size_t i = 0; i < headers.size(); ++i) {
                    char c = headers[i];
                    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
                    lower += c;
                }
                size_t cl = lower.find("content-length:");
                if (cl != std::string::npos) {
                    size_t eol = lower.find('\n', cl);
                    std::string val = lower.substr(cl + 15, eol == std::string::npos ? std::string::npos : eol - cl - 15);
                    std::string num;
                    for (char c : val) if (c >= '0' && c <= '9') num += c;
                    contentLength = static_cast<size_t>(std::atoll(num.c_str()));
                }
            }
        }
        if (headerDone && raw.size() >= headerEnd + contentLength) break;
        if (raw.size() > (8u << 20)) break;  // 8 MB cap
    }

    HttpRequest req;
    // Parse request line.
    size_t eol = raw.find("\r\n");
    if (eol == std::string::npos) eol = raw.find('\n');
    if (eol != std::string::npos) {
        std::string line = raw.substr(0, eol);
        // Trim trailing \r.
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t sp1 = line.find(' ');
        size_t sp2 = sp1 == std::string::npos ? std::string::npos : line.find(' ', sp1 + 1);
        if (sp1 != std::string::npos && sp2 != std::string::npos) {
            req.method = line.substr(0, sp1);
            req.path = line.substr(sp1 + 1, sp2 - sp1 - 1);
        }
    }
    parseQuery(req.path, req.params);
    if (headerDone && headerEnd > 0) {
        parseHeaders(raw.substr(0, headerEnd), req.headers);
    }
    if (contentLength > 0 && raw.size() >= headerEnd) {
        req.body = raw.substr(headerEnd, contentLength);
    }

    HttpResponse resp = dispatch(req);
    auto t1 = std::chrono::steady_clock::now();
    std::fprintf(stderr, "[DBG] handleClient method=%s path=%s -> status=%d bodySize=%zu elapsedMs=%lld\n", req.method.c_str(), req.path.c_str(), resp.status, resp.body.size(), (long long)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
    fflush(stderr);

    std::string out = "HTTP/1.1 " + std::to_string(resp.status) + " ";
    out += std::string(resp.status == 200 ? "OK"
                        : resp.status == 400 ? "Bad Request"
                        : resp.status == 404 ? "Not Found"
                        : resp.status == 500 ? "Internal Server Error"
                        : "OK") + "\r\n";
    out += "Content-Type: " + resp.contentType + "\r\n";
    out += "Content-Length: " + std::to_string(resp.body.size()) + "\r\n";
    out += "Connection: close\r\n\r\n";
    out += resp.body;

    size_t sent = 0;
    while (sent < out.size()) {
        int w = send(fd, out.data() + sent, static_cast<int>(out.size() - sent), 0);
        if (w <= 0) break;
        sent += static_cast<size_t>(w);
    }
    closeSocket(fd);
}

}  // namespace lodestar::api
