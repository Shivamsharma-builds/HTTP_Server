cmake_minimum_required(VERSION 3.16)

project(http_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(http_server
    src/main.cpp
    src/server.cpp
    src/socket.cpp
    src/http_request.cpp
    src/http_response.cpp
)

target_include_directories(http_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/header
)

if(WIN32)
    target_link_libraries(http_server PRIVATE ws2_32)
endif()