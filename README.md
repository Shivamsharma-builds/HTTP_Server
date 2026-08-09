# HTTP Server Using C++ From Scratch

A lightweight HTTP server built from scratch in **C++**, with the goal
of understanding how web servers work at a low level without relying on
high-level HTTP server frameworks.

## 🚀 Project Highlights

### HTTP Web Server
**C++, HTTP Protocol, Non-Blocking I/O** · **March 2025**

- Built a lightweight HTTP server from the ground up in C++, supporting **GET, POST, and DELETE** requests along with cookies, CGI integration, file uploads and downloads, and static resource delivery.
- Designed the server around **non-blocking I/O**, allowing it to process read and write operations concurrently across multiple connected clients.
- Focused on understanding the internals of HTTP communication, socket programming, request handling, and concurrent client management.

## 📌 Project Overview

This project implements the core building blocks of an HTTP server using
C++ and operating-system networking APIs.

The main objective is to learn how an HTTP request travels from a client
to a server, how TCP sockets are used for communication, how HTTP
requests are parsed, and how HTTP responses are constructed and sent
back to the client.

### Request Flow

``` text
Client (Browser / Postman / curl)
              |
              v
        TCP Connection
              |
              v
       Socket / accept()
              |
              v
       Read HTTP Request
              |
              v
       Parse HTTP Request
              |
              v
       Route / Process Request
              |
              v
      Build HTTP Response
              |
              v
       Send Response
              |
              v
            Client
```

## 🎯 Goals

-   Understand TCP socket programming in C++.
-   Understand how HTTP works internally.
-   Build an HTTP server without using a web-server framework.
-   Learn how browsers and API clients communicate with servers.
-   Parse HTTP request lines and headers.
-   Generate valid HTTP responses.
-   Understand client-server communication at the socket level.
-   Create a foundation for adding routing, concurrency, and other
    server features.

## 🛠️ Technologies

-   **Language:** C++
-   **Protocol:** HTTP/1.1
-   **Networking:** TCP/IP sockets
-   **Build System:** CMake
-   **Testing:** Browser, Postman, and `curl`
-   **Platform:** Linux / Unix-like systems

> Windows support can be added using the Winsock API (`winsock2.h`) with
> platform-specific socket initialization and cleanup.

## 📂 Project Structure

``` text
http-server/
├── include/
│   ├── server.hpp
│   ├── socket.hpp
│   ├── http_request.hpp
│   └── http_response.hpp
│
├── src/
│   ├── main.cpp
│   ├── server.cpp
│   ├── socket.cpp
│   ├── http_request.cpp
│   └── http_response.cpp
│
├── public/
│   └── index.html
│
├── tests/
│   └── ...
│
├── CMakeLists.txt
└── README.md
```

The exact structure may change as the project evolves.

## ⚙️ How an HTTP Server Works

At a high level, the server performs these steps:

### 1. Create a Socket

A TCP socket is created so the server can communicate with clients.

``` cpp
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
```

### 2. Bind the Socket

The socket is associated with an IP address and port.

``` cpp
bind(server_fd, ...);
```

For local development, the server can listen on:

``` text
127.0.0.1:8080
```

### 3. Listen for Connections

The server starts listening for incoming TCP connections.

``` cpp
listen(server_fd, SOMAXCONN);
```

### 4. Accept a Client

When a client connects, the server accepts the connection.

``` cpp
int client_fd = accept(server_fd, ...);
```

### 5. Read the HTTP Request

The server reads bytes sent by the client.

A simple request might look like:

``` http
GET / HTTP/1.1
Host: localhost:8080
Connection: close
```

### 6. Parse the Request

The server extracts information such as:

-   HTTP method
-   Request path
-   HTTP version
-   Headers
-   Request body

For example:

``` text
Method: GET
Path: /
Version: HTTP/1.1
```

### 7. Generate an HTTP Response

The server creates a response such as:

``` http
HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 13
Connection: close

Hello, World!
```

### 8. Send the Response

The response is written back to the client through the TCP socket.

### 9. Close the Connection

For a simple implementation, the connection can be closed after the
response is sent.

## 🚀 Getting Started

### To compile the files in windows

```
g++ -std=c++14 -Wall -Wextra -Iheader src/main.cpp src/server.cpp src/socket.cpp src/http_request.cpp src/http_response.cpp -o server.exe -lws2_32          
```

### Prerequisites

Make sure the following tools are installed:

-   C++ compiler with C++17 or later
-   CMake
-   Git
-   `curl` (optional, for testing)

Check your compiler:

``` bash
g++ --version
```

Check CMake:

``` bash
cmake --version
```

## 🔨 Build

Clone the repository:

``` bash
git clone <your-repository-url>
cd http-server
```

Create a build directory:

``` bash
mkdir build
cd build
```

Configure the project:

``` bash
cmake ..
```

Build the project:

``` bash
cmake --build .
```

## ▶️ Run the Server

After building, run the server executable:

``` bash
./http_server
```

If the server uses port `8080`, you should see something similar to:

``` text
HTTP Server started on http://127.0.0.1:8080
Waiting for connections...
```

## 🌐 Test Using a Browser

Open:

``` text
http://127.0.0.1:8080
```

The browser will establish a TCP connection and send an HTTP request to
the server.

## 🧪 Test Using curl

Send a GET request:

``` bash
curl http://127.0.0.1:8080/
```

Example response:

``` text
Hello, World!
```

You can also inspect the HTTP response headers:

``` bash
curl -i http://127.0.0.1:8080/
```

Example:

``` http
HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 13
Connection: close

Hello, World!
```

## 📮 Test Using Postman

Create a new request in Postman:

``` text
Method: GET
URL: http://127.0.0.1:8080/
```

Click **Send**.

The server should receive the request and return an HTTP response.

## 📡 Supported HTTP Concepts

The implementation can be extended to support:

  Feature                     Status
  --------------------------- --------
  TCP socket server           ✅
  HTTP request parsing        ✅
  HTTP response generation    ✅
  GET requests                ✅
  HTTP headers                ✅
  Static file serving         🔄
  POST requests               🔄
  Request body parsing        🔄
  Routing                     🔄
  Query parameters            🔄
  Concurrent clients          🔄
  Keep-Alive                  🔄
  Chunked transfer encoding   🔄
  HTTPS/TLS                   🔄

`✅` indicates implemented functionality and `🔄` indicates planned or
extendable functionality. Update this table as the project develops.

## 🧩 Example Request

``` http
GET /hello HTTP/1.1
Host: localhost:8080
User-Agent: curl/8.x
Accept: */*
```

The server can parse this into an internal representation:

``` cpp
HttpRequest request;

request.method  = "GET";
request.path    = "/hello";
request.version = "HTTP/1.1";
```

## 📤 Example Response

``` http
HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 12
Connection: close

Hello World!
```

## 🏗️ Core Components

### Server

Responsible for:

-   Starting the server.
-   Creating the listening socket.
-   Accepting clients.
-   Managing the request/response lifecycle.

### Socket

Responsible for low-level TCP communication:

-   `socket()`
-   `bind()`
-   `listen()`
-   `accept()`
-   `recv()` / `read()`
-   `send()` / `write()`
-   `close()`

### HTTP Request

Represents data received from the client:

``` text
Method
Path
HTTP Version
Headers
Body
```

### HTTP Response

Represents the response sent to the client:

``` text
Status Code
Status Message
Headers
Body
```

## 🔐 Error Handling

The server should return appropriate HTTP status codes when possible.

Common status codes include:

  Status Code                   Meaning
  ----------------------------- -----------------------------------
  `200 OK`                      Request succeeded
  `201 Created`                 Resource created
  `400 Bad Request`             Invalid HTTP request
  `404 Not Found`               Requested resource does not exist
  `405 Method Not Allowed`      HTTP method is not supported
  `500 Internal Server Error`   Unexpected server error

## 🧵 Future Improvements

Possible improvements include:

1.  Support multiple clients simultaneously.
2.  Add a thread-per-connection model.
3.  Implement a thread pool.
4.  Add non-blocking sockets.
5.  Implement `select()`, `poll()`, or `epoll()`.
6.  Add HTTP routing.
7.  Serve static files.
8.  Support POST and other HTTP methods.
9.  Parse request bodies.
10. Add JSON responses.
11. Implement HTTP keep-alive.
12. Add logging.
13. Add configuration files.
14. Add unit and integration tests.
15. Add HTTPS using TLS.
16. Improve security and request validation.

## 📚 What This Project Teaches

By building this server from scratch, you can gain practical knowledge
of:

-   TCP/IP networking
-   Socket programming
-   Client-server architecture
-   HTTP/1.1 fundamentals
-   Request parsing
-   Response construction
-   Operating-system networking APIs
-   Blocking vs. non-blocking I/O
-   Concurrency
-   Network error handling
-   Low-level debugging

## 🐛 Debugging

When debugging the server, useful tools include:

``` bash
curl -v http://127.0.0.1:8080/
```

The `-v` option displays the TCP/HTTP communication details.

You can also check whether the server is listening on the expected port:

``` bash
ss -ltnp | grep 8080
```

On systems using `netstat`:

``` bash
netstat -ltnp | grep 8080
```

## 🤝 Contributing

Contributions and improvements are welcome.

A typical workflow is:

``` bash
git checkout -b feature/my-feature
```

Make your changes, test them, and then create a pull request.

## 📄 License

This project is intended for learning and experimentation.

Add your preferred license here, for example:

``` text
MIT License
```

## ⭐ Project Motivation

The purpose of this project is not to build a production-ready web
server immediately. The primary goal is to understand what happens
underneath frameworks and web-server libraries.

Instead of starting with a framework such as Express, FastAPI, or a C++
HTTP library, this project starts with TCP sockets and gradually builds
the HTTP layer on top.

``` text
TCP Socket
    ↓
TCP Connection
    ↓
HTTP Request
    ↓
HTTP Parser
    ↓
Request Handler
    ↓
HTTP Response
    ↓
TCP Socket
```

This approach provides a hands-on understanding of how modern web
servers communicate with clients.
