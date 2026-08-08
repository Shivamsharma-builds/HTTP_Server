#include "server.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {
constexpr int kMaxEvents = 64;
constexpr size_t kReadChunk = 65536;
constexpr size_t kMaxBodySize = 20 * 1024 * 1024;  // 20 MB upload cap
}  // namespace

Server::Server(std::string host, int port, std::string publicDir, std::string uploadDir)
    : host_(std::move(host)),
      port_(port),
      publicDir_(std::move(publicDir)),
      uploadDir_(std::move(uploadDir)),
      epollFd_(-1) {
    fs::create_directories(uploadDir_);
}

bool Server::start() {
    if (!serverSocket_.create()) return false;
    if (!serverSocket_.bindTo(host_, port_)) return false;
    if (!serverSocket_.listenOn()) return false;

    Socket::setNonBlocking(serverSocket_.getFd());

    epollFd_ = epoll_create1(0);
    if (epollFd_ < 0) {
        std::cerr << "epoll_create1() failed: " << strerror(errno) << "\n";
        return false;
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = serverSocket_.getFd();
    if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, serverSocket_.getFd(), &ev) < 0) {
        std::cerr << "epoll_ctl() failed to add server socket: " << strerror(errno) << "\n";
        return false;
    }

    std::cout << "HTTP Server started on http://" << (host_.empty() ? "127.0.0.1" : host_) << ":"
              << port_ << "\n";
    std::cout << "Waiting for connections...\n";
    return true;
}

void Server::run() {
    std::array<epoll_event, kMaxEvents> events{};

    while (true) {
        int n = epoll_wait(epollFd_, events.data(), kMaxEvents, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            std::cerr << "epoll_wait() failed: " << strerror(errno) << "\n";
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            if (fd == serverSocket_.getFd()) {
                handleNewConnections();
            } else if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                closeClient(fd);
            } else if (events[i].events & EPOLLIN) {
                handleClientReadable(fd);
            }
        }
    }
}

void Server::handleNewConnections() {
    while (true) {
        int clientFd = serverSocket_.acceptConnection();
        if (clientFd < 0) break;  // no more pending connections (EAGAIN/EWOULDBLOCK)

        Socket::setNonBlocking(clientFd);

        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = clientFd;
        epoll_ctl(epollFd_, EPOLL_CTL_ADD, clientFd, &ev);

        clientBuffers_[clientFd] = "";
    }
}

void Server::handleClientReadable(int clientFd) {
    char buf[kReadChunk];
    std::string& buffer = clientBuffers_[clientFd];

    while (true) {
        ssize_t bytesRead = read(clientFd, buf, sizeof(buf));
        if (bytesRead > 0) {
            buffer.append(buf, static_cast<size_t>(bytesRead));
        } else if (bytesRead == 0) {
            // Peer closed the connection.
            closeClient(clientFd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;  // drained for now
            closeClient(clientFd);
            return;
        }
    }

    auto expected = HttpRequest::computeExpectedLength(buffer);
    if (!expected.has_value()) return;  // headers not fully received yet

    if (buffer.size() > kMaxBodySize) {
        HttpResponse res(413);
        res.setHeader("Content-Type", "text/plain");
        res.setBody("Payload Too Large\n");
        std::string out = res.toString();
        write(clientFd, out.c_str(), out.size());
        closeClient(clientFd);
        return;
    }

    if (buffer.size() < *expected) return;  // still waiting on body bytes

    HttpRequest req = HttpRequest::parse(buffer);
    HttpResponse res = routeRequest(req);
    std::string out = res.toString();

    size_t totalSent = 0;
    while (totalSent < out.size()) {
        ssize_t sent = write(clientFd, out.c_str() + totalSent, out.size() - totalSent);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            break;
        }
        totalSent += static_cast<size_t>(sent);
    }

    closeClient(clientFd);  // Connection: close semantics (no keep-alive yet)
}

void Server::closeClient(int clientFd) {
    epoll_ctl(epollFd_, EPOLL_CTL_DEL, clientFd, nullptr);
    close(clientFd);
    clientBuffers_.erase(clientFd);
}

HttpResponse Server::routeRequest(const HttpRequest& req) {
    HttpResponse res(200);

    std::cout << req.method << " " << req.path << " -> ";

    if (req.method == "GET") {
        handleGet(req, res);
    } else if (req.method == "POST") {
        handlePost(req, res);
    } else if (req.method == "DELETE") {
        handleDelete(req, res);
    } else {
        res.setStatus(405);
        res.setHeader("Content-Type", "text/plain");
        res.setHeader("Allow", "GET, POST, DELETE");
        res.setBody("405 Method Not Allowed\n");
    }

    std::cout << res.toString().substr(0, 15) << "...\n";
    return res;
}

void Server::handleGet(const HttpRequest& req, HttpResponse& res) {
    std::string path = req.path;

    // Simple request-count cookie to demonstrate cookie support.
    int visits = 1;
    auto it = req.cookies.find("visits");
    if (it != req.cookies.end()) {
        try {
            visits = std::stoi(it->second) + 1;
        } catch (...) {
            visits = 1;
        }
    }
    res.setCookie("visits", std::to_string(visits), 3600);

    if (path == "/") path = "/index.html";

    if (path.rfind("/files/", 0) == 0) {
        std::string filename = sanitizePath(path.substr(std::string("/files/").size()));
        std::string fullPath = uploadDir_ + "/" + filename;

        if (!fs::exists(fullPath) || !fs::is_regular_file(fullPath)) {
            res.setStatus(404);
            res.setHeader("Content-Type", "text/plain");
            res.setBody("404 Not Found: " + path + "\n");
            return;
        }

        std::ifstream file(fullPath, std::ios::binary);
        std::ostringstream ss;
        ss << file.rdbuf();

        res.setStatus(200);
        res.setHeader("Content-Type", getMimeType(fullPath));
        res.setHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        res.setBody(ss.str());
        return;
    }

    if (path.rfind("/cgi-bin/", 0) == 0) {
        if (tryServeCgi(req, res)) return;
    }

    if (tryServeStaticFile(path, res)) return;

    res.setStatus(404);
    res.setHeader("Content-Type", "text/plain");
    res.setBody("404 Not Found: " + req.path + "\n");
}

void Server::handlePost(const HttpRequest& req, HttpResponse& res) {
    if (req.path == "/upload") {
        std::string filename;
        auto it = req.headers.find("x-filename");
        if (it != req.headers.end() && !it->second.empty()) {
            filename = sanitizePath(it->second);
        } else {
            auto now = std::chrono::system_clock::now().time_since_epoch().count();
            filename = "upload_" + std::to_string(now) + ".bin";
        }

        std::string fullPath = uploadDir_ + "/" + filename;
        std::ofstream out(fullPath, std::ios::binary);
        if (!out) {
            res.setStatus(500);
            res.setHeader("Content-Type", "text/plain");
            res.setBody("500 Internal Server Error: could not write file\n");
            return;
        }
        out.write(req.body.data(), static_cast<std::streamsize>(req.body.size()));
        out.close();

        res.setStatus(201);
        res.setHeader("Content-Type", "application/json");
        res.setHeader("Location", "/files/" + filename);
        res.setBody("{\"status\":\"created\",\"file\":\"" + filename + "\",\"bytes\":" +
                    std::to_string(req.body.size()) + "}\n");
        return;
    }

    if (req.path.rfind("/cgi-bin/", 0) == 0) {
        if (tryServeCgi(req, res)) return;
    }

    res.setStatus(404);
    res.setHeader("Content-Type", "text/plain");
    res.setBody("404 Not Found: " + req.path + "\n");
}

void Server::handleDelete(const HttpRequest& req, HttpResponse& res) {
    if (req.path.rfind("/files/", 0) == 0) {
        std::string filename = sanitizePath(req.path.substr(std::string("/files/").size()));
        std::string fullPath = uploadDir_ + "/" + filename;

        if (!fs::exists(fullPath)) {
            res.setStatus(404);
            res.setHeader("Content-Type", "text/plain");
            res.setBody("404 Not Found: " + req.path + "\n");
            return;
        }

        fs::remove(fullPath);
        res.setStatus(204);
        res.setHeader("Content-Type", "text/plain");
        return;
    }

    res.setStatus(404);
    res.setHeader("Content-Type", "text/plain");
    res.setBody("404 Not Found: " + req.path + "\n");
}

bool Server::tryServeStaticFile(const std::string& path, HttpResponse& res) {
    std::string safePath = sanitizePath(path);
    std::string fullPath = publicDir_ + "/" + safePath;

    if (!fs::exists(fullPath) || !fs::is_regular_file(fullPath)) return false;

    std::ifstream file(fullPath, std::ios::binary);
    if (!file) return false;

    std::ostringstream ss;
    ss << file.rdbuf();

    res.setStatus(200);
    res.setHeader("Content-Type", getMimeType(fullPath));
    res.setBody(ss.str());
    return true;
}

bool Server::tryServeCgi(const HttpRequest& req, HttpResponse& res) {
    // Minimal CGI hook: executes public/cgi-bin/<script> and captures stdout.
    // Real CGI (env vars, stdin body, headers-from-script) is intentionally
    // simplified here as a learning stub.
    std::string scriptRelative = req.path.substr(std::string("/cgi-bin/").size());
    std::string scriptPath = publicDir_ + "/cgi-bin/" + sanitizePath(scriptRelative);

    if (!fs::exists(scriptPath) || !fs::is_regular_file(scriptPath)) return false;

    std::string cmd = scriptPath + " 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        res.setStatus(500);
        res.setHeader("Content-Type", "text/plain");
        res.setBody("500 Internal Server Error: CGI execution failed\n");
        return true;
    }

    std::ostringstream output;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0) {
        output.write(buf, static_cast<std::streamsize>(n));
    }
    pclose(pipe);

    res.setStatus(200);
    res.setHeader("Content-Type", "text/plain");
    res.setBody(output.str());
    return true;
}

std::string Server::sanitizePath(const std::string& path) {
    // Strips ".." segments and leading slashes to prevent path traversal.
    std::string cleaned;
    std::istringstream ss(path);
    std::string segment;
    while (std::getline(ss, segment, '/')) {
        if (segment.empty() || segment == ".") continue;
        if (segment == "..") continue;  // drop traversal attempts entirely
        if (!cleaned.empty()) cleaned += "/";
        cleaned += segment;
    }
    return cleaned;
}

std::string Server::getMimeType(const std::string& path) {
    static const std::unordered_map<std::string, std::string> mimeTypes = {
        {".html", "text/html"},        {".htm", "text/html"},
        {".css", "text/css"},          {".js", "application/javascript"},
        {".json", "application/json"}, {".png", "image/png"},
        {".jpg", "image/jpeg"},        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},         {".svg", "image/svg+xml"},
        {".txt", "text/plain"},        {".pdf", "application/pdf"},
        {".ico", "image/x-icon"},
    };

    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return "application/octet-stream";

    std::string ext = path.substr(dot);
    auto it = mimeTypes.find(ext);
    return it != mimeTypes.end() ? it->second : "application/octet-stream";
}
