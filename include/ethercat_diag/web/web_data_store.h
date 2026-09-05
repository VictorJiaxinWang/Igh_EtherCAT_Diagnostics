#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

struct WebDataResult
{
    bool success{};
    std::string body;
    std::string error;
};

class WebDataStore
{
public:
    explicit WebDataStore(std::filesystem::path directory);

    WebDataResult readStatus() const;
    WebDataResult readRecentEvents(std::size_t limit) const;

private:
    std::filesystem::path directory_;
};
