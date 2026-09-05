#include "ethercat_diag/web/http_router.h"

#include <charconv>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace
{

HttpResponse jsonResponse(
    int status,
    std::string reason,
    std::string body)
{
    return {
        status,
        std::move(reason),
        "application/json; charset=utf-8",
        std::move(body)};
}

HttpResponse errorResponse(
    int status,
    const std::string& reason,
    const std::string& message)
{
    return jsonResponse(
        status,
        reason,
        "{\"error\":\"" + message + "\"}\n");
}

std::optional<std::size_t> eventLimit(const std::string& target)
{
    if (target == "/api/events")
    {
        return 50U;
    }

    constexpr const char prefix[] = "/api/events?limit=";
    if (target.compare(0U, sizeof(prefix) - 1U, prefix) != 0)
    {
        return std::nullopt;
    }

    const std::string text = target.substr(sizeof(prefix) - 1U);
    unsigned int value{};
    const auto parsed = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value);
    if (text.empty() ||
        parsed.ec != std::errc{} ||
        parsed.ptr != text.data() + text.size() ||
        value < 1U ||
        value > 200U)
    {
        return std::nullopt;
    }

    return static_cast<std::size_t>(value);
}

HttpResponse readAsset(
    const std::filesystem::path& path,
    const std::string& content_type)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return errorResponse(404, "Not Found", "resource not found");
    }

    const std::string body{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    if (input.bad())
    {
        return errorResponse(404, "Not Found", "resource not found");
    }

    return {200, "OK", content_type, body};
}

}

std::string serializeHttpResponse(const HttpResponse& response)
{
    std::ostringstream output;
    output << "HTTP/1.1 " << response.status << ' ' << response.reason << "\r\n"
           << "Content-Type: " << response.content_type << "\r\n"
           << "Content-Length: " << response.body.size() << "\r\n"
           << "Cache-Control: no-store\r\n"
           << "Connection: close\r\n"
           << "\r\n"
           << response.body;
    return output.str();
}

HttpRouter::HttpRouter(
    std::filesystem::path data_directory,
    std::filesystem::path assets_directory)
    : data_store_(std::move(data_directory)),
      assets_directory_(std::move(assets_directory))
{
}

HttpResponse HttpRouter::route(const HttpRequest& request) const
{
    if (request.method != "GET")
    {
        return errorResponse(405, "Method Not Allowed", "method not allowed");
    }

    if (request.target == "/healthz")
    {
        return jsonResponse(200, "OK", "{\"status\":\"ok\"}\n");
    }

    if (request.target == "/api/status")
    {
        const WebDataResult result = data_store_.readStatus();
        if (!result.success)
        {
            return errorResponse(503, "Service Unavailable", result.error);
        }
        return jsonResponse(200, "OK", result.body);
    }

    if (request.target.compare(0U, 11U, "/api/events") == 0)
    {
        const std::optional<std::size_t> limit = eventLimit(request.target);
        if (!limit)
        {
            return errorResponse(400, "Bad Request", "invalid event limit");
        }
        const WebDataResult result = data_store_.readRecentEvents(*limit);
        if (!result.success)
        {
            return errorResponse(503, "Service Unavailable", result.error);
        }
        return jsonResponse(200, "OK", result.body);
    }

    if (request.target == "/" || request.target == "/index.html")
    {
        return readAsset(
            assets_directory_ / "index.html",
            "text/html; charset=utf-8");
    }
    if (request.target == "/styles.css")
    {
        return readAsset(
            assets_directory_ / "styles.css",
            "text/css; charset=utf-8");
    }
    if (request.target == "/app.js")
    {
        return readAsset(
            assets_directory_ / "app.js",
            "application/javascript; charset=utf-8");
    }

    return errorResponse(404, "Not Found", "resource not found");
}
