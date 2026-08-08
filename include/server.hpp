#pragma once

#include <string>
#include <unordered_map>

#include "http_request.hpp"
#include "http_response.hpp"
#include "socket.hpp"

// Non-blocking, single-threaded, epoll-driven HTTP server.
// Handles GET / POST / DELETE, cookies, static file serving,
// file upload/download, and a minimal CGI hook.
class Server {
public:
    Server(std::string host, int port, std::string publicDir, std::string uploadDir);

    // Binds, listens, and runs the epoll event loop. Blocks forever.
    bool start();
    void run();

private:
    void handleNewConnections();
    void handleClientReadable(int clientFd);
    void closeClient(int clientFd);

    HttpResponse routeRequest(const HttpRequest& req);
    void handleGet(const HttpRequest& req, HttpResponse& res);
    void handlePost(const HttpRequest& req, HttpResponse& res);
    void handleDelete(const HttpRequest& req, HttpResponse& res);

    bool tryServeStaticFile(const std::string& path, HttpResponse& res);
    bool tryServeCgi(const HttpRequest& req, HttpResponse& res);
    static std::string getMimeType(const std::string& path);
    static std::string sanitizePath(const std::string& path);

    Socket serverSocket_;
    std::string host_;
    int port_;
    std::string publicDir_;
    std::string uploadDir_;
    int epollFd_;

    // Per-connection read buffer, keyed by client fd.
    std::unordered_map<int, std::string> clientBuffers_;
};
