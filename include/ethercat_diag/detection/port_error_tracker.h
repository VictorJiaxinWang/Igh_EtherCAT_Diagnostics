#pragma once

#include "ethercat_diag/common/diag_types.h"
#include "ethercat_diag/esc/port_error_decoder.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class PortErrorTracker
{
public:
    std::vector<FaultEvent> process(
        std::uint64_t timestamp_ms,
        int slave_position,
        const std::string& slave_name,
        const PortErrorCounters& current);

    std::vector<FaultEvent> process(
        std::uint64_t timestamp_ms,
        int master_index,
        int slave_position,
        int slave_alias,
        int slave_relative_position,
        const std::string& slave_name,
        const PortErrorCounters& current);

private:
    struct Baseline
    {
        std::string slave_name;
        PortErrorCounters counters;
    };

    std::unordered_map<std::string, Baseline> baselines_;
};
