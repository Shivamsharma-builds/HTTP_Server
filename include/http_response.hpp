#pragma once

#include <string>
#include <unordered_map>
#include <vector>

// Represents an HTTP response and knows how to serialize itself to wire format.
class HttpResponse {
public:
    explicit HttpResponse(int statusCode = 200);

    void setStatus(int code);
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);

    // Adds a Set-Cookie header. maxAgeSeconds < 0 means a session cookie.
    void setCookie(const std::string& name, const std::string& value, int maxAgeSeconds = -1);

    std::string toString() const;

    static std::string statusMessage(int code);

private:
    int statusCode_;
    std::unordered_map<std::string, std::string> headers_;
    std::vector<std::string> setCookies_;
    std::string body_;
};
