#pragma once

#include <string>
#include <unordered_map>

#if __has_include(<optional>)
#include <optional>
namespace http_util = std;
#elif __has_include(<experimental/optional>)
#include <experimental/optional>
namespace http_util = std::experimental;
#else
#error "Optional support is required for this project."
#endif

using namespace std;

// Represents a parsed HTTP request.
struct HttpRequest {
    string method;
    string path;         // path only, query string stripped
    string version;
    unordered_map<string, string> headers;      // lower-cased keys
    unordered_map<string, string> queryParams;
    unordered_map<string, string> cookies;
    string body;

    // Parses a raw HTTP request (request-line + headers + body) into an HttpRequest.
    // Assumes the caller has already buffered a complete request (including a body
    // of Content-Length bytes, if present).
    static HttpRequest parse(const string& raw);

    // Given the raw buffer received so far, returns the number of bytes the full
    // request will occupy once fully received, or std::nullopt if not yet known
    // (headers not fully received yet).
    static http_util::optional<size_t> computeExpectedLength(const string& raw);

    http_util::optional<string> getHeader(const string& key) const;
};
