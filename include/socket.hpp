#pragma once

#include <string>
#include <sys/socket.h>

// RAII wrapper around a POSIX TCP socket.
// Handles creation, binding, listening, accepting, and non-blocking mode.
class Socket {
public:
    Socket();
    ~Socket();

    // Non-copyable (owns a raw fd), movable.
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    bool create();
    bool bindTo(const std::string& host, int port);
    bool listenOn(int backlog = SOMAXCONN);

    // Returns the accepted client fd, or -1 on error / would-block.
    int acceptConnection();

    // Marks any fd (server or client) as non-blocking.
    static void setNonBlocking(int fd);

    int getFd() const { return fd_; }
    void closeSocket();

private:
    int fd_;
};
