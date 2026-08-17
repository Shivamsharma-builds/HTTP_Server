#include <cstdlib>
#include <iostream>
#include <string>

#include "server.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    // Listen on all network interfaces so Koyeb can reach the server.
    string host = "0.0.0.0";

    // Koyeb provides the PORT environment variable.
    int port = 8080;

    if (const char* envPort = getenv("PORT")) {
        try {
            port = stoi(envPort);
        } catch (...) {
            cerr << "Invalid PORT environment variable: "
                 << envPort << "\n";
            return 1;
        }
    }

    string publicDir = "public";
    string uploadDir = "uploads";

    // Optional CLI overrides:
    // ./http_server [port] [publicDir]
    if (argc >= 2) {
        try {
            port = stoi(argv[1]);
        } catch (...) {
            cerr << "Invalid port: " << argv[1] << "\n";
            return 1;
        }
    }

    if (argc >= 3) {
        publicDir = argv[2];
    }

    cout << "Starting HTTP server...\n";
    cout << "Host: " << host << "\n";
    cout << "Port: " << port << "\n";
    cout << "Public directory: " << publicDir << "\n";

    Server server(host, port, publicDir, uploadDir);

    if (!server.start()) {
        cerr << "Failed to start server.\n";
        return 1;
    }

    server.run();

    return 0;
}