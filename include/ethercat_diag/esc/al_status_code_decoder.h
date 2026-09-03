#pragma once

#include <cstdint>
#include <string_view>

struct AlStatusCodeInfo
{
    std::uint16_t raw{};
    std::string_view description{};
    bool known{};
};

enum class AlStatusCodeContext
{
    NoError,
    ActiveError,
    ActiveWarning,
    Inactive
};

AlStatusCodeContext classifyAlStatusCode(
    std::uint16_t code,
    bool error_indication,
    bool warning_indication
);

AlStatusCodeInfo decodeAlStatusCode(std::uint16_t raw);
