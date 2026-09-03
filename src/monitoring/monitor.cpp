#include "ethercat_diag/monitoring/monitor.h"

#include <thread>
#include <utility>

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
    if (config_.master_index < 0 ||
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
        NetworkSnapshot snapshot;

        const bool read_success =
            reader_(
                config_.master_index,
                snapshot);

        if (read_success)
        {
            handler_(snapshot);
        }

        next_deadline += config_.interval;

        wait_until_(next_deadline);
    }

    return true;
}