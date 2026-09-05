#include "ethercat_diag/web/http_server.h"

#include "ethercat_diag/web/http_types.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sstream>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <utility>

namespace
{

constexpr std::size_t max_request_size = 8192U;

HttpResponse requestError(
    int status,
    const std::string& reason,
    const std::string& message)
{
    return {
        status,
        reason,
        "application/json; charset=utf-8",
        "{\"error\":\"" + message + "\"}\n"};
}

void sendAll(int socket_fd, const std::string& contents)
{
    std::size_t sent = 0U;
    while (sent < contents.size())
    {
        const ssize_t result = ::send(
            socket_fd,
            contents.data() + sent,
            contents.size() - sent,
            MSG_NOSIGNAL);
        if (result > 0)
        {
            sent += static_cast<std::size_t>(result);
            continue;
        }
        if (result < 0 && errno == EINTR)
        {
            continue;
        }
        break;
    }
}

}

HttpServer::HttpServer(
    const HttpRouter& router,
    std::string bind_address,
    std::uint16_t port)
    : router_(router),
      bind_address_(std::move(bind_address)),
      requested_port_(port)
{
}

int HttpServer::run(const std::function<bool()>& should_stop)
{
    last_error_.clear();
    const int listen_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket < 0)
    {
        last_error_ = "socket: " + std::string(std::strerror(errno));
        return 1;
    }

    const auto fail = [&](const std::string& operation) {
        last_error_ = operation + ": " + std::string(std::strerror(errno));
        ::close(listen_socket);
        bound_port_.store(0U);
        return 1;
    };

    int reuse_address = 1;
    if (::setsockopt(
            listen_socket,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse_address,
            sizeof(reuse_address)) != 0)
    {
        return fail("setsockopt");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(requested_port_);
    if (::inet_pton(
            AF_INET,
            bind_address_.c_str(),
            &address.sin_addr) != 1)
    {
        last_error_ = "invalid IPv4 bind address: " + bind_address_;
        ::close(listen_socket);
        return 1;
    }

    if (::bind(
            listen_socket,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) != 0)
    {
        return fail("bind");
    }

    sockaddr_in bound_address{};
    socklen_t bound_length = sizeof(bound_address);
    if (::getsockname(
            listen_socket,
            reinterpret_cast<sockaddr*>(&bound_address),
            &bound_length) != 0)
    {
        return fail("getsockname");
    }
    bound_port_.store(ntohs(bound_address.sin_port));

    if (::listen(listen_socket, 16) != 0)
    {
        return fail("listen");
    }

    while (!should_stop())
    {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(listen_socket, &read_set);
        timeval timeout{};
        timeout.tv_usec = 200000;

        const int ready = ::select(
            listen_socket + 1,
            &read_set,
            nullptr,
            nullptr,
            &timeout);
        if (ready < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return fail("select");
        }
        if (ready == 0)
        {
            continue;
        }

        const int client_socket = ::accept(
            listen_socket,
            nullptr,
            nullptr);
        if (client_socket < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return fail("accept");
        }

        serveClient(client_socket);
        ::close(client_socket);
    }

    ::close(listen_socket);
    bound_port_.store(0U);
    return 0;
}

std::uint16_t HttpServer::boundPort() const
{
    return bound_port_.load();
}

const std::string& HttpServer::lastError() const
{
    return last_error_;
}

void HttpServer::serveClient(int client_socket) const
{
    timeval timeout{};
    timeout.tv_sec = 2;
    ::setsockopt(
        client_socket,
        SOL_SOCKET,
        SO_RCVTIMEO,
        &timeout,
        sizeof(timeout));
    ::setsockopt(
        client_socket,
        SOL_SOCKET,
        SO_SNDTIMEO,
        &timeout,
        sizeof(timeout));

    std::string request_text;
    char buffer[1024];
    while (request_text.find("\r\n\r\n") == std::string::npos &&
           request_text.size() < max_request_size)
    {
        const ssize_t received = ::recv(
            client_socket,
            buffer,
            sizeof(buffer),
            0);
        if (received > 0)
        {
            request_text.append(buffer, static_cast<std::size_t>(received));
            continue;
        }
        if (received < 0 && errno == EINTR)
        {
            continue;
        }
        break;
    }

    HttpResponse response;
    if (request_text.size() >= max_request_size)
    {
        response = requestError(
            431,
            "Request Header Fields Too Large",
            "request headers are too large");
    }
    else
    {
        const std::size_t line_end = request_text.find("\r\n");
        std::string method;
        std::string target;
        std::string version;
        std::string extra;
        std::istringstream line(
            line_end == std::string::npos
                ? std::string{}
                : request_text.substr(0U, line_end));
        line >> method >> target >> version >> extra;

        if (method.empty() ||
            target.empty() ||
            version.compare(0U, 5U, "HTTP/") != 0 ||
            !extra.empty())
        {
            response = requestError(400, "Bad Request", "invalid HTTP request");
        }
        else
        {
            response = router_.route({method, target});
        }
    }

    sendAll(client_socket, serializeHttpResponse(response));
}
