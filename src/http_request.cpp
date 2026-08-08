#include "http_request.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Splits "path?a=1&b=2" into path + queryParams.
void parsePathAndQuery(const std::string& target, std::string& path,
                        std::unordered_map<std::string, std::string>& queryParams) {
    size_t qPos = target.find('?');
    if (qPos == std::string::npos) {
        path = target;
        return;
    }
    path = target.substr(0, qPos);
    std::string query = target.substr(qPos + 1);

    std::istringstream qs(query);
    std::string pair;
    while (std::getline(qs, pair, '&')) {
        size_t eq = pair.find('=');
        if (eq == std::string::npos) {
            queryParams[pair] = "";
        } else {
            queryParams[pair.substr(0, eq)] = pair.substr(eq + 1);
        }
    }
}

void parseCookies(const std::string& cookieHeader,
                   std::unordered_map<std::string, std::string>& cookies) {
    std::istringstream cs(cookieHeader);
    std::string pair;
    while (std::getline(cs, pair, ';')) {
        size_t eq = pair.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(pair.substr(0, eq));
        std::string value = trim(pair.substr(eq + 1));
        cookies[key] = value;
    }
}

}  // namespace

HttpRequest HttpRequest::parse(const std::string& raw) {
    HttpRequest req;

    size_t headerEnd = raw.find("\r\n\r\n");
    std::string headerSection = headerEnd == std::string::npos ? raw : raw.substr(0, headerEnd);

    std::istringstream stream(headerSection);
    std::string line;

    // Request line: METHOD PATH VERSION
    if (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream lineStream(line);
        std::string target;
        lineStream >> req.method >> target >> req.version;
        parsePathAndQuery(target, req.path, req.queryParams);
    }

    // Headers
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key = toLower(trim(line.substr(0, colon)));
        std::string value = trim(line.substr(colon + 1));
        req.headers[key] = value;

        if (key == "cookie") {
            parseCookies(value, req.cookies);
        }
    }

    // Body (everything after the blank line)
    if (headerEnd != std::string::npos) {
        req.body = raw.substr(headerEnd + 4);
    }

    return req;
}

std::optional<size_t> HttpRequest::computeExpectedLength(const std::string& raw) {
    size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return std::nullopt;  // headers not fully received yet
    }

    size_t headerBytes = headerEnd + 4;

    // Look for Content-Length (case-insensitive) within the header section.
    std::string headerSection = raw.substr(0, headerEnd);
    std::string lower = toLower(headerSection);
    size_t clPos = lower.find("content-length:");
    if (clPos == std::string::npos) {
        return headerBytes;  // no body expected
    }

    size_t lineEnd = headerSection.find("\r\n", clPos);
    std::string valueStr = headerSection.substr(
        clPos + std::string("content-length:").size(),
        (lineEnd == std::string::npos ? headerSection.size() : lineEnd) - (clPos + 15));
    valueStr = trim(valueStr);

    size_t contentLength = 0;
    try {
        contentLength = static_cast<size_t>(std::stoul(valueStr));
    } catch (...) {
        contentLength = 0;
    }

    return headerBytes + contentLength;
}

std::optional<std::string> HttpRequest::getHeader(const std::string& key) const {
    auto it = headers.find(toLower(key));
    if (it == headers.end()) return std::nullopt;
    return it->second;
}
