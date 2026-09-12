#pragma once

#include "ethercat_diag/common/diag_types.h"

#include <chrono>
#include <functional>
#include <vector>

struct MonitorConfig
{
    int master_index{0};
    std::vector<int> master_indices;

    std::chrono::milliseconds interval{
        1000
    };
};

class Monitor
{
public:
    using Clock =
        std::chrono::steady_clock;

    using SnapshotReader =
        std::function<
            bool(int, NetworkSnapshot&)>;

    using SnapshotHandler =
        std::function<
            void(const NetworkSnapshot&)>;

    using WaitUntil =
        std::function<
            void(Clock::time_point)>;

    using ContinuePredicate =
        std::function<bool()>;

    Monitor(
        MonitorConfig config,
        SnapshotReader reader,
        SnapshotHandler handler);

    Monitor(
        MonitorConfig config,
        SnapshotReader reader,
        SnapshotHandler handler,
        WaitUntil wait_until);

    bool run(
        const ContinuePredicate& should_continue);

private:
    MonitorConfig config_;
    SnapshotReader reader_;
    SnapshotHandler handler_;
    WaitUntil wait_until_;
};
