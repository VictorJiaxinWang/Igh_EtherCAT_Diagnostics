#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct WebOptions
{
    std::string bind_address{"0.0.0.0"};
    std::uint16_t port{8080};
    std::filesystem::path data_directory{"logs"};
    std::filesystem::path assets_directory{"web"};
};

struct WebOptionsParseResult
{
    bool success{};
    bool show_help{};
    WebOptions options;
    std::string error;
};

WebOptionsParseResult parseWebOptions(
    const std::vector<std::string>& arguments);

std::string webOptionsUsage();
