#pragma once

#include "ethercat_diag/common/diag_types.h"
#include "ethercat_diag/detection/port_error_tracker.h"
#include "ethercat_diag/esc/port_error_reader.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

struct PortErrorMonitorResult
{
    bool attempted{};
    int slave_position{-1};
    std::vector<FaultEvent> events;
    std::string error;
};

class PortErrorMonitor
{
public:
    using ReadFunction =
        std::function<PortErrorReadResult(int, int)>;

    explicit PortErrorMonitor(int master_index = 0);

    PortErrorMonitor(
        int master_index,
        PortErrorReader reader);

    PortErrorMonitor(
        int master_index,
        ReadFunction read);

    PortErrorMonitorResult process(
        const NetworkSnapshot& snapshot);

private:
    int master_index_{};
    ReadFunction read_;
    PortErrorTracker tracker_;
    std::size_t next_online_index_{};
};
