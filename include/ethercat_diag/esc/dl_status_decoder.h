#pragma once

#include <array>
#include <cstdint>

struct DlPortStatus {
    bool physical_link{};
    bool loop_closed{};
    bool communication_established{};
};

struct DlStatusInfo {
    std::uint16_t raw{};
    bool pdi_operational{};
    bool pdi_watchdog_reloaded{};
    bool enhanced_link_detection{};
    std::array<DlPortStatus, 4> ports{};
};

DlStatusInfo decodeDlStatus(std::uint16_t raw);
