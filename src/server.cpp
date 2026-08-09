#include "server.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <sys/stat.h>

#if defined(__MINGW32__) || defined(_WIN32)
extern "C" FILE* _popen(const char* command, const char* mode);
extern "C" int _pclose(FILE* stream);
#endif

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/select.h>
#endif

namespace fs_compat {
bool pathExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool isRegularFile(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return (st.st_mode & S_IFMT) == S_IFREG;
}

bool createDirectory(const std::string& path) {
#ifdef _WIN32
    if (_mkdir(path.c_str()) == 0) return true;
#else
    if (mkdir(path.c_str(), 0777) == 0) return true;
#endif
    return errno == EEXIST;
}

bool removeFile(const std::string& path) {
    return std::remove(path.c_str()) == 0;
}
}

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
    fs_compat::createDirectory(uploadDir_);
}

bool Server::start() {
    if (!serverSocket_.create()) return false;
    if (!serverSocket_.bindTo(host_, port_)) return false;
    if (!serverSocket_.listenOn()) return false;

    Socket::setNonBlocking(serverSocket_.getFd());

    std::cout << "HTTP Server started on http://" << (host_.empty() ? "127.0.0.1" : host_) << ":"
              << port_ << "\n";
    std::cout << "Waiting for connections...\n";
    return true;
}

void Server::run() {
    while (true) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(serverSocket_.getFd(), &readSet);

        int maxFd = serverSocket_.getFd();
        for (const auto& entry : clientBuffers_) {
            int fd = entry.first;
            FD_SET(fd, &readSet);
            if (fd > maxFd) maxFd = fd;
        }

#ifdef _WIN32
        int n = select(0, &readSet, nullptr, nullptr, nullptr);
#else
        int n = select(maxFd + 1, &readSet, nullptr, nullptr, nullptr);
#endif
        if (n < 0) {
            if (errno == EINTR) continue;
            std::cerr << "select() failed: " << strerror(errno) << "\n";
            break;
        }

        if (FD_ISSET(serverSocket_.getFd(), &readSet)) {
            handleNewConnections();
        }

        std::vector<int> readyClients;
        for (const auto& entry : clientBuffers_) {
            if (FD_ISSET(entry.first, &readSet)) {
                readyClients.push_back(entry.first);
            }
        }

        for (int fd : readyClients) {
            handleClientReadable(fd);
        }
    }
}

void Server::handleNewConnections() {
    while (true) {
        int clientFd = serverSocket_.acceptConnection();
        if (clientFd < 0) break;  // no more pending connections (EAGAIN/EWOULDBLOCK)

        Socket::setNonBlocking(clientFd);
        clientBuffers_[clientFd] = "";
    }
}

void Server::handleClientReadable(int clientFd) {
    char buf[kReadChunk];
    std::string& buffer = clientBuffers_[clientFd];

    while (true) {
#ifdef _WIN32
        int bytesRead = recv(clientFd, buf, sizeof(buf), 0);
#else
        ssize_t bytesRead = read(clientFd, buf, sizeof(buf));
#endif
        if (bytesRead > 0) {
            buffer.append(buf, static_cast<size_t>(bytesRead));
        } else if (bytesRead == 0) {
            closeClient(clientFd);
            return;
        } else {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK || err == WSAEINTR) break;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
#endif
            closeClient(clientFd);
            return;
        }
    }

    auto expected = HttpRequest::computeExpectedLength(buffer);
    if (!expected) return;  // headers not fully received yet

    if (buffer.size() > kMaxBodySize) {
        HttpResponse res(413);
        res.setHeader("Content-Type", "text/plain");
        res.setBody("Payload Too Large\n");
        std::string out = res.toString();
#ifdef _WIN32
        send(clientFd, out.c_str(), static_cast<int>(out.size()), 0);
#else
        write(clientFd, out.c_str(), out.size());
#endif
        closeClient(clientFd);
        return;
    }

    if (buffer.size() < *expected) return;  // still waiting on body bytes

    HttpRequest req = HttpRequest::parse(buffer);
    HttpResponse res = routeRequest(req);
    std::string out = res.toString();

    size_t totalSent = 0;
    while (totalSent < out.size()) {
#ifdef _WIN32
        int sent = send(clientFd, out.c_str() + totalSent, static_cast<int>(out.size() - totalSent), 0);
        if (sent < 0) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK || err == WSAEINTR) continue;
            break;
        }
#else
        ssize_t sent = write(clientFd, out.c_str() + totalSent, out.size() - totalSent);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            break;
        }
#endif
        totalSent += static_cast<size_t>(sent);
    }

    closeClient(clientFd);  // Connection: close semantics (no keep-alive yet)
}

void Server::closeClient(int clientFd) {
#ifdef _WIN32
    closesocket(clientFd);
#else
    close(clientFd);
#endif
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

        if (!fs_compat::pathExists(fullPath) || !fs_compat::isRegularFile(fullPath)) {
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

        if (!fs_compat::pathExists(fullPath)) {
            res.setStatus(404);
            res.setHeader("Content-Type", "text/plain");
            res.setBody("404 Not Found: " + req.path + "\n");
            return;
        }

        fs_compat::removeFile(fullPath);
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

    if (!fs_compat::pathExists(fullPath) || !fs_compat::isRegularFile(fullPath)) return false;

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

    if (!fs_compat::pathExists(scriptPath) || !fs_compat::isRegularFile(scriptPath)) return false;

    std::string cmd;
    FILE* pipe = nullptr;
#if defined(__MINGW32__) || defined(_WIN32)
    // On Windows, batch files might need to be invoked via cmd.exe.
    // This also helps if the script path contains spaces.
    cmd = "cmd.exe /c \"" + scriptPath + "\" 2>&1";
    pipe = _popen(cmd.c_str(), "r");
#else
    cmd = scriptPath + " 2>&1";
    pipe = popen(cmd.c_str(), "r");
#endif
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
#if defined(__MINGW32__) || defined(_WIN32)
    _pclose(pipe);
#else
    pclose(pipe);
#endif

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
