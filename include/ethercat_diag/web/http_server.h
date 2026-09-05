#pragma once

#include "ethercat_diag/web/http_router.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

class HttpServer
{
public:
    HttpServer(
        const HttpRouter& router,
        std::string bind_address,
        std::uint16_t port);

    int run(const std::function<bool()>& should_stop);
    std::uint16_t boundPort() const;
    const std::string& lastError() const;

private:
    void serveClient(int client_socket) const;

    const HttpRouter& router_;
    std::string bind_address_;
    std::uint16_t requested_port_{};
    std::atomic<std::uint16_t> bound_port_{0U};
    std::string last_error_;
};
