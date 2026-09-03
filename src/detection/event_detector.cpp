#include "ethercat_diag/detection/event_detector.h"

std::vector<FaultEvent> EventDetector::process(
    const NetworkSnapshot& current)
{
    std::vector<FaultEvent> events;

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
        bool still_exists = false;

        for (const SlaveSnapshot& current_slave :
            current.slaves)
        {
            if (current_slave.position ==
                old_slave.position)
            {
                still_exists = true;
                break;
            }
        }

        if (!still_exists)
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
                        " was lost"
                });
        }
    }

    for (const SlaveSnapshot& current_slave :
        current.slaves)
    {
        for (const SlaveSnapshot& old_slave :
            previous_->slaves)
        {
            if (current_slave.position !=
                old_slave.position)
            {
                continue;
            }

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
                            " state changed"
                    });
            }

            break;
        }
    }

    previous_ = current;

    return events;
}