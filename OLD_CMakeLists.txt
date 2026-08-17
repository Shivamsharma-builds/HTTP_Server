cmake_minimum_required(VERSION 3.15)
project(http_server CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

add_executable(http_server
    src/main.cpp
    src/server.cpp
    src/socket.cpp
    src/http_request.cpp
    src/http_response.cpp
)

if(WIN32)
    target_link_libraries(http_server PRIVATE ws2_32)
endif()

target_include_directories(http_server PRIVATE ${CMAKE_SOURCE_DIR}/header)

# Copy the public/ (static assets + cgi-bin) directory next to the built
# binary so `./http_server` can find it via a relative path out of the box.
add_custom_command(TARGET http_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/public
        $<TARGET_FILE_DIR:http_server>/public
)
