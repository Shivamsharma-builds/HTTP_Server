#include "socket.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <unistd.h>

Socket::Socket() : fd_(-1) {}

Socket::~Socket() {
    closeSocket();
}

bool Socket::create() {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        std::cerr << "socket(): failed to create socket: " << strerror(errno) << "\n";
        return false;
    }

    // Allow quick restart of the server on the same port.
    int opt = 1;
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
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
    } else if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "bindTo(): invalid host address '" << host << "'\n";
        return false;
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
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) flags = 0;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void Socket::closeSocket() {
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}
