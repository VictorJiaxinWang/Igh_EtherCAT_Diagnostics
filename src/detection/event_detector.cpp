#include "ethercat_diag/detection/event_detector.h"

#include <algorithm>

std::vector<FaultEvent> EventDetector::process(
    const NetworkSnapshot& current)
{
    std::vector<FaultEvent> events;

    const auto findCurrent = [&current](const SlaveSnapshot& wanted) {
        return std::find_if(
            current.slaves.begin(), current.slaves.end(),
            [&wanted](const SlaveSnapshot& candidate) {
                return sameStableIdentity(wanted, candidate);
            });
    };

    if (!previous_)
    {
        previous_ = current;
        return events;
    }

    const bool old_link_up =
        previous_->master.link_up;

    const bool new_link_up =
        current.master.link_up;

    if (old_link_up && !new_link_up)
    {
        events.push_back(
            FaultEvent{
                current.master.timestamp_ms,
                EventType::MASTER_LINK_DOWN,
                -1,
                1,
                0,
                "Master link changed from UP to DOWN"
            });
    }

    if (!old_link_up && new_link_up)
    {
        events.push_back(
            FaultEvent{
                current.master.timestamp_ms,
                EventType::MASTER_LINK_UP,
                -1,
                0,
                1,
                "Master link changed from DOWN to UP"
            }
        );
    }

    const int old_slave_count = previous_->master.slave_count;
    const int new_slave_count = current.master.slave_count;

    if (old_slave_count != new_slave_count)
    {
        events.push_back(
            FaultEvent{
                current.master.timestamp_ms,
                EventType::SLAVE_COUNT_CHANGED,
                -1,
                old_slave_count,
                new_slave_count,
                "Slave count changed"
            }
        );
    }

    for (const SlaveSnapshot& old_slave :
     previous_->slaves)
    {
        if (!stableIdentity(old_slave).valid())
        {
            continue;
        }
        if (findCurrent(old_slave) == current.slaves.end())
        {
            events.push_back(
                FaultEvent{
                    current.master.timestamp_ms,
                    EventType::SLAVE_LOST,
                    old_slave.position,
                    1,
                    0,
                    "Slave " +
                        std::to_string(
                            old_slave.position) +
                        " (alias " + std::to_string(old_slave.alias) +
                        ":" + std::to_string(old_slave.relative_position) +
                        ") was lost",
                    -1,
                    current.master.master_index,
                    old_slave.alias,
                    old_slave.relative_position});
        }
    }

    for (const SlaveSnapshot& current_slave :
        current.slaves)
    {
        const auto old = std::find_if(
            previous_->slaves.begin(), previous_->slaves.end(),
            [&current_slave](const SlaveSnapshot& candidate) {
                return sameStableIdentity(current_slave, candidate);
            });
        if (old != previous_->slaves.end())
        {
            const SlaveSnapshot& old_slave = *old;

            if (current_slave.state != old_slave.state)
            {
                events.push_back(
                    FaultEvent{
                        current.master.timestamp_ms,
                        EventType::SLAVE_STATE_CHANGED,
                        current_slave.position,
                        static_cast<int>(old_slave.state),
                        static_cast<int>(current_slave.state),
                        "Slave " +
                            std::to_string(
                                current_slave.position) +
                            " state changed",
                        -1,
                        current.master.master_index,
                        current_slave.alias,
                        current_slave.relative_position});
            }

        }
    }

    for (FaultEvent& event : events)
    {
        if (event.master_index < 0)
        {
            event.master_index = current.master.master_index;
        }
    }

    previous_ = current;

    return events;
}
