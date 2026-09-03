#pragma once

#include <cstdint>
#include <string_view>

enum class EscAlState : std::uint8_t
{
    Init,
    PreOp,
    Boot,
    SafeOp,
    Op,
    Unknown
};

struct AlStatusInfo
{
    std::uint16_t raw{};
    EscAlState state{EscAlState::Unknown};
    bool error_indication{false};
    bool explicit_device_id_loaded{false};
    bool warning_indication{false};
};

AlStatusInfo decodeAlStatus(std::uint16_t raw);
std::string_view alStateName(EscAlState state);

