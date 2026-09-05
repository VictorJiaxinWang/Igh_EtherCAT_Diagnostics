#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

inline constexpr std::uint16_t esc_port_error_base_address = 0x0300U;
inline constexpr std::size_t esc_port_error_block_size = 20U;

struct PortErrorCounter
{
    std::uint8_t invalid_frame{};
    std::uint8_t rx_error{};
    std::uint8_t forwarded_rx_error{};
    std::uint8_t lost_link{};
};

struct PortErrorCounters
{
    std::array<PortErrorCounter, 4U> ports{};
};

struct PortErrorDecodeResult
{
    bool success{};
    PortErrorCounters counters;
    std::string error;
};

PortErrorDecodeResult decodePortErrorCounters(
    const std::vector<std::uint8_t>& bytes);
