#pragma once

#include <optional>
#include <string>
#include <unordered_map>

// Represents a parsed HTTP request.
struct HttpRequest {
    std::string method;
    std::string path;         // path only, query string stripped
    std::string version;
    std::unordered_map<std::string, std::string> headers;      // lower-cased keys
    std::unordered_map<std::string, std::string> queryParams;
    std::unordered_map<std::string, std::string> cookies;
    std::string body;

    // Parses a raw HTTP request (request-line + headers + body) into an HttpRequest.
    // Assumes the caller has already buffered a complete request (including a body
    // of Content-Length bytes, if present).
    static HttpRequest parse(const std::string& raw);

    // Given the raw buffer received so far, returns the number of bytes the full
    // request will occupy once fully received, or std::nullopt if not yet known
    // (headers not fully received yet).
    static std::optional<size_t> computeExpectedLength(const std::string& raw);

    std::optional<std::string> getHeader(const std::string& key) const;
};
