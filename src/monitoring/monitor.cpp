#include "ethercat_diag/monitoring/monitor.h"

#include <thread>
#include <utility>
#include <algorithm>
#include <set>

Monitor::Monitor(
    MonitorConfig config,
    SnapshotReader reader,
    SnapshotHandler handler)
    : config_(config),
      reader_(std::move(reader)),
      handler_(std::move(handler)),
      wait_until_(
          [](Clock::time_point deadline)
          {
              std::this_thread::sleep_until(
                  deadline);
          })
{
}

Monitor::Monitor(
    MonitorConfig config,
    SnapshotReader reader,
    SnapshotHandler handler,
    WaitUntil wait_until)
    : config_(config),
      reader_(std::move(reader)),
      handler_(std::move(handler)),
      wait_until_(std::move(wait_until))
{
}

bool Monitor::run(
    const ContinuePredicate& should_continue)
{
    const std::vector<int> masters = config_.master_indices.empty()
        ? std::vector<int>{config_.master_index}
        : config_.master_indices;
    const std::set<int> unique(masters.begin(), masters.end());
    if (masters.empty() || unique.size() != masters.size() ||
        std::any_of(masters.begin(), masters.end(),
            [](int value) { return value < 0; }) ||
        config_.interval <= std::chrono::milliseconds::zero() ||
        !reader_ ||
        !handler_ ||
        !wait_until_ ||
        !should_continue) {
        return false;
    }

    Clock::time_point next_deadline =
        Clock::now();

    while (should_continue())
    {
        for (const int master_index : masters)
        {
            NetworkSnapshot snapshot;
            const bool read_success = reader_(master_index, snapshot);
            if (read_success)
            {
                handler_(snapshot);
            }
        }

        next_deadline += config_.interval;

        wait_until_(next_deadline);
    }

    return true;
}
