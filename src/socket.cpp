#include "socket.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <cerrno>
#else
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#include <cstring>
#include <iostream>

Socket::Socket() : fd_(-1) {}

Socket::~Socket() {
    closeSocket();
}

bool Socket::create() {
#ifdef _WIN32
    static bool winsockInitialized = false;
    if (!winsockInitialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup() failed\n";
            return false;
        }
        winsockInitialized = true;
    }
#endif

    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        std::cerr << "socket(): failed to create socket: " << strerror(errno) << "\n";
        return false;
    }

    int opt = 1;
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0) {
        std::cerr << "setsockopt(SO_REUSEADDR) failed: " << strerror(errno) << "\n";
    }

    return true;
}

bool Socket::bindTo(const std::string& host, int port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (host.empty() || host == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        unsigned long ip = inet_addr(host.c_str());
        if (ip == INADDR_NONE && std::strcmp(host.c_str(), "255.255.255.255") != 0) {
            std::cerr << "bindTo(): invalid host address '" << host << "'\n";
            return false;
        }
        addr.sin_addr.s_addr = ip;
    }

    if (bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind(): failed: " << strerror(errno) << "\n";
        return false;
    }

    return true;
}

bool Socket::listenOn(int backlog) {
    if (listen(fd_, backlog) < 0) {
        std::cerr << "listen(): failed: " << strerror(errno) << "\n";
        return false;
    }
    return true;
}

int Socket::acceptConnection() {
    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);
    int clientFd = accept(fd_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
    return clientFd;
}

void Socket::setNonBlocking(int fd) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) flags = 0;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

void Socket::closeSocket() {
    if (fd_ >= 0) {
#ifdef _WIN32
        closesocket(fd_);
#else
        close(fd_);
#endif
        fd_ = -1;
    }
}
