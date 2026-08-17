#include <cstdlib>
#include <iostream>
#include <string>

#include "server.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    string host = "0.0.0.0";
    int port = 10000;

    if (const char* envPort = getenv("PORT")) {
        try {
            port = stoi(envPort);
        } catch (...) {
            cerr << "Invalid PORT: " << envPort << "\n";
            return 1;
        }
    }

    string publicDir = "public";
    string uploadDir = "uploads";

    if (argc >= 2)
        port = stoi(argv[1]);

    if (argc >= 3)
        publicDir = argv[2];

    cout << "Starting server on " << host << ":" << port << "\n";

    Server server(host, port, publicDir, uploadDir);

    if (!server.start()) {
        cerr << "Failed to start server.\n";
        return 1;
    }

    server.run();

    return 0;
}