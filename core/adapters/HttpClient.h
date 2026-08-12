// core/adapters/HttpClient.h
// Minimal blocking HTTP/1.1 client over BSD/Winsock sockets used by the network
// adapters (Llm, Python, and the RF/SCPI adapters when they talk HTTP). No
// external HTTP dependency is required; this keeps the adapter layer
// header-only/dependency-free on both Windows and Linux.

#pragma once

#include <string>

namespace lodestar::adapters {

class HttpClient {
public:
    struct Response {
        int status = 0;      // HTTP status code (0 on transport failure)
        std::string reason;  // e.g. "OK"
        std::string body;    // response body (headers stripped)
        bool ok() const { return status >= 200 && status < 300; }
    };

    // Perform a blocking request. On transport failure (unreachable host, timeout,
    // socket error) returns a Response with status 0 and stores a message in
    // errOut (non-null). Headers is a CRLF-separated string of extra header lines
    // (excluding Host/Content-Length, which are added automatically).
    static Response request(const std::string& host, int port,
                            const std::string& method, const std::string& path,
                            const std::string& body,
                            const std::string& contentType,
                            int timeoutMs, std::string* errOut,
                            const std::string& extraHeaders = "");
};

}  // namespace lodestar::adapters
