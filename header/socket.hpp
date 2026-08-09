#pragma once

#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif

// RAII wrapper around a TCP socket.
// Handles creation, binding, listening, accepting, and non-blocking mode.
class Socket {
public:
    Socket();
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    bool create();
    bool bindTo(const std::string& host, int port);
    bool listenOn(int backlog = SOMAXCONN);

    int acceptConnection();
    static void setNonBlocking(int fd);

    int getFd() const { return fd_; }
    void closeSocket();

private:
    int fd_;
};
