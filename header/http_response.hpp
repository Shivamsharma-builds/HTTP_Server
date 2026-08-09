#pragma once

#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

// Represents an HTTP response and knows how to serialize itself to wire format.
class HttpResponse {
public:
    explicit HttpResponse(int statusCode = 200);

    void setStatus(int code);
    void setHeader(const string& key, const string& value);
    void setBody(const string& body);

    // Adds a Set-Cookie header. maxAgeSeconds < 0 means a session cookie.
    void setCookie(const string& name, const string& value, int maxAgeSeconds = -1);

    string toString() const;

    static string statusMessage(int code);

private:
    int statusCode_;
    unordered_map<string, string> headers_;
    vector<string> setCookies_;
    string body_;
};
