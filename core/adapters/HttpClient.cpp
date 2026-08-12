// core/adapters/HttpClient.cpp
// Minimal blocking HTTP/1.1 client over Winsock/BSD sockets.

#include "core/adapters/HttpClient.h"

#include <cstdio>
#include <cstring>
#include <chrono>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using SockLen_t = int;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SockLen_t = socklen_t;
#endif

namespace lodestar::adapters {

namespace {

// RAII winsock lifecycle (Windows only).
struct WsaGuard {
#ifdef _WIN32
    WsaGuard() {
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
    }
    ~WsaGuard() { WSACleanup(); }
#endif
    WsaGuard(const WsaGuard&) = delete;
    WsaGuard& operator=(const WsaGuard&) = delete;
};

void closeSocket(int fd) {
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
}

void setSocketTimeout(int fd, int timeoutMs) {
#ifdef _WIN32
    // On Windows, SO_RCVTIMEO/SO_SNDTIMEO expect a DWORD timeout in
    // milliseconds (not a struct timeval as on POSIX). Passing a timeval here
    // would be read as a tiny millisecond value and cause spurious timeouts.
    DWORD ms = static_cast<DWORD>(timeoutMs);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
#else
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#endif
}

bool sendAll(int fd, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int n = send(fd, data + sent, static_cast<int>(len - sent), 0);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

std::string recvAll(int fd) {
    std::string out;
    char buf[4096];
    while (true) {
        int n = recv(fd, buf, sizeof(buf), 0);
        if (n < 0) {
#ifdef _WIN32
            std::fprintf(stderr, "[DBG] recvAll n=%d err=%d\n", n, WSAGetLastError()); fflush(stderr);
#else
            std::fprintf(stderr, "[DBG] recvAll n=%d errno=%d\n", n, errno); fflush(stderr);
#endif
        } else {
            std::fprintf(stderr, "[DBG] recvAll n=%d\n", n); fflush(stderr);
        }
        if (n <= 0) break;
        out.append(buf, static_cast<size_t>(n));
        if (out.size() > (64u << 20)) break;  // 64 MB safety cap
    }
    return out;
}

// Split the raw HTTP response into a body starting at the first blank line.
std::string splitBody(const std::string& raw, size_t* headerLenOut) {
    size_t pos = raw.find("\r\n\r\n");
    if (pos == std::string::npos) {
        pos = raw.find("\n\n");
        if (pos == std::string::npos) {
            *headerLenOut = raw.size();
            return "";
        }
        *headerLenOut = pos + 2;
        return raw.substr(pos + 2);
    }
    *headerLenOut = pos + 4;
    return raw.substr(pos + 4);
}

}  // namespace

HttpClient::Response HttpClient::request(const std::string& host, int port,
                                         const std::string& method,
                                         const std::string& path,
                                         const std::string& body,
                                         const std::string& contentType,
                                         int timeoutMs, std::string* errOut,
                                         const std::string& extraHeaders) {
    Response resp;
    WsaGuard wsa;

    if (host.empty() || port <= 0) {
        if (errOut) *errOut = "invalid host/port";
        return resp;
    }
    if (timeoutMs <= 0) timeoutMs = 5000;

    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* res = nullptr;
    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%d", port);
    if (getaddrinfo(host.c_str(), portStr, &hints, &res) != 0 || !res) {
        if (errOut) *errOut = "getaddrinfo failed for host '" + host + "'";
        return resp;
    }

    int fd = -1;
    for (addrinfo* ai = res; ai; ai = ai->ai_next) {
        fd = static_cast<int>(socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol));
        if (fd < 0) continue;
        if (connect(fd, ai->ai_addr, static_cast<SockLen_t>(ai->ai_addrlen)) == 0) break;
        closeSocket(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    if (fd < 0) {
        if (errOut) *errOut = "connection refused / unreachable: " + host + ":" + portStr;
        return resp;
    }

    setSocketTimeout(fd, timeoutMs);

    std::string reqPath = path.empty() ? "/" : path;
    std::fprintf(stderr, "[DBG] HttpClient connecting %s:%d for %s %s\n", host.c_str(), port, method.c_str(), path.c_str()); fflush(stderr);
    std::string request = method + " " + reqPath + " HTTP/1.1\r\n";
    request += "Host: " + host + ":" + portStr + "\r\n";
    request += "Connection: close\r\n";
    if (!body.empty()) {
        request += "Content-Type: " + (contentType.empty() ? "application/json" : contentType) + "\r\n";
        request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }
    // Append caller-supplied extra header lines (e.g. "X-API-Key: <key>").
    if (!extraHeaders.empty()) {
        request += extraHeaders;
        if (extraHeaders.back() != '\n') request += "\r\n";
    }
    request += "\r\n";
    request += body;

    bool ok = sendAll(fd, request.data(), request.size());
    std::fprintf(stderr, "[DBG] HttpClient sent %s %s ok=%d\n", method.c_str(), path.c_str(), ok ? 1 : 0); fflush(stderr);
    auto rt0 = std::chrono::steady_clock::now();
    std::string raw;
    if (ok) raw = recvAll(fd);
    auto rt1 = std::chrono::steady_clock::now();
    std::fprintf(stderr, "[DBG] HttpClient recv %s %s rawSize=%zu recvMs=%lld\n", method.c_str(), path.c_str(), raw.size(), (long long)std::chrono::duration_cast<std::chrono::milliseconds>(rt1 - rt0).count()); fflush(stderr);
    closeSocket(fd);

    if (!ok || raw.empty()) {
        if (errOut) *errOut = "HTTP send/recv failed (timeout or reset): " + host + ":" + portStr;
        return resp;
    }

    // Parse status line: "HTTP/1.1 200 OK".
    size_t sp1 = raw.find(' ');
    size_t sp2 = sp1 == std::string::npos ? std::string::npos : raw.find(' ', sp1 + 1);
    if (sp1 != std::string::npos && sp2 != std::string::npos) {
        resp.status = std::atoi(raw.substr(sp1 + 1, sp2 - sp1 - 1).c_str());
        size_t eol = raw.find("\r\n", sp2);
        if (eol != std::string::npos) resp.reason = raw.substr(sp2 + 1, eol - sp2 - 1);
    } else {
        resp.status = 0;
        if (errOut) *errOut = "malformed HTTP response from " + host + ":" + portStr;
        return resp;
    }

    size_t headerLen = 0;
    resp.body = splitBody(raw, &headerLen);
    std::fprintf(stderr, "[DBG] HttpClient %s %s -> status=%d bodySize=%zu rawSize=%zu\n", method.c_str(), path.c_str(), resp.status, resp.body.size(), raw.size());
    fflush(stderr);
    return resp;
}

}  // namespace lodestar::adapters
