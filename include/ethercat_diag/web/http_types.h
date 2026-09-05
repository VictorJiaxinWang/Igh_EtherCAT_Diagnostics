#pragma once

#include <string>

struct HttpRequest
{
    std::string method;
    std::string target;
};

struct HttpResponse
{
    int status{};
    std::string reason;
    std::string content_type;
    std::string body;
};

std::string serializeHttpResponse(const HttpResponse& response);
