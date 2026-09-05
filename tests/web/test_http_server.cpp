#include "ethercat_diag/web/http_server.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace
{

std::string request(std::uint16_t port, const std::string& wire_request)
{
    const int socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    assert(socket_fd >= 0);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    assert(::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) == 1);
    assert(::connect(
               socket_fd,
               reinterpret_cast<sockaddr*>(&address),
               sizeof(address)) == 0);
    assert(::send(
               socket_fd,
               wire_request.data(),
               wire_request.size(),
               0) == static_cast<ssize_t>(wire_request.size()));

    std::string response;
    char buffer[1024];
    for (;;)
    {
        const ssize_t received = ::recv(socket_fd, buffer, sizeof(buffer), 0);
        if (received <= 0)
        {
            break;
        }
        response.append(buffer, static_cast<std::size_t>(received));
    }
    ::close(socket_fd);
    return response;
}

void testRealLoopbackRequestAndGracefulStop()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("ethercat-http-server-" + std::to_string(::getpid()));
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root / "data");
    std::filesystem::create_directories(root / "assets");
    std::ofstream(root / "assets" / "index.html") << "dashboard\n";

    HttpRouter router(root / "data", root / "assets");
    HttpServer server(router, "127.0.0.1", 0U);
    std::atomic_bool stop{false};
    int run_result = -1;
    std::thread thread([&]() {
        run_result = server.run([&stop]() { return stop.load(); });
    });

    for (int attempt = 0; attempt < 100 && server.boundPort() == 0U; ++attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(server.boundPort() != 0U);

    const std::string response = request(
        server.boundPort(),
        "GET /healthz HTTP/1.1\r\nHost: localhost\r\n\r\n");
    assert(response.find("HTTP/1.1 200 OK\r\n") == 0U);
    assert(response.find("{\"status\":\"ok\"}\n") != std::string::npos);

    stop.store(true);
    thread.join();
    assert(run_result == 0);

    std::filesystem::remove_all(root, error);
}

}

int main()
{
    testRealLoopbackRequestAndGracefulStop();
    return 0;
}
