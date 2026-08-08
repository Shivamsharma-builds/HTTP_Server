#include "http_response.hpp"

#include <sstream>

HttpResponse::HttpResponse(int statusCode) : statusCode_(statusCode) {
    headers_["Server"] = "cpp-http-server/0.1";
    headers_["Connection"] = "close";
}

void HttpResponse::setStatus(int code) { statusCode_ = code; }

void HttpResponse::setHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

void HttpResponse::setBody(const std::string& body) { body_ = body; }

void HttpResponse::setCookie(const std::string& name, const std::string& value, int maxAgeSeconds) {
    std::ostringstream cookie;
    cookie << name << "=" << value << "; Path=/";
    if (maxAgeSeconds >= 0) {
        cookie << "; Max-Age=" << maxAgeSeconds;
    }
    setCookies_.push_back(cookie.str());
}

std::string HttpResponse::statusMessage(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

std::string HttpResponse::toString() const {
    std::ostringstream out;
    out << "HTTP/1.1 " << statusCode_ << " " << statusMessage(statusCode_) << "\r\n";

    for (const auto& [key, value] : headers_) {
        out << key << ": " << value << "\r\n";
    }
    out << "Content-Length: " << body_.size() << "\r\n";

    for (const auto& cookie : setCookies_) {
        out << "Set-Cookie: " << cookie << "\r\n";
    }

    out << "\r\n" << body_;
    return out.str();
}
