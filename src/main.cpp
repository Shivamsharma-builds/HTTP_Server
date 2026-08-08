#include <iostream>
#include <string>

#include "server.hpp"

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string publicDir = "public";
    std::string uploadDir = "uploads";

    // Optional CLI overrides: ./http_server [port] [publicDir]
    if (argc >= 2) port = std::stoi(argv[1]);
    if (argc >= 3) publicDir = argv[2];

    Server server(host, port, publicDir, uploadDir);

    if (!server.start()) {
        std::cerr << "Failed to start server.\n";
        return 1;
    }

    server.run();
    return 0;
}
