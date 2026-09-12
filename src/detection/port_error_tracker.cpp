#include "ethercat_diag/detection/port_error_tracker.h"

namespace
{

bool anyCounterDecreased(
    const PortErrorCounters& previous,
    const PortErrorCounters& current)
{
    for (std::size_t port = 0U;
         port < current.ports.size();
         ++port)
    {
        const PortErrorCounter& old_value = previous.ports[port];
        const PortErrorCounter& new_value = current.ports[port];

        if (new_value.invalid_frame < old_value.invalid_frame ||
            new_value.rx_error < old_value.rx_error ||
            new_value.forwarded_rx_error <
                old_value.forwarded_rx_error ||
            new_value.lost_link < old_value.lost_link)
        {
            return true;
        }
    }

    return false;
}

void appendEventIfIncreased(
    std::vector<FaultEvent>& events,
    std::uint64_t timestamp_ms,
    int slave_position,
    int master_index,
    int slave_alias,
    int slave_relative_position,
    int port_position,
    EventType type,
    std::uint8_t previous,
    std::uint8_t current,
    const std::string& counter_name)
{
    if (current <= previous)
    {
        return;
    }

    events.push_back(FaultEvent{
        timestamp_ms,
        type,
        slave_position,
        static_cast<int>(previous),
        static_cast<int>(current),
        "Slave " + std::to_string(slave_position) +
            " port " + std::to_string(port_position) +
            " " + counter_name + " increased",
        port_position,
        master_index,
        slave_alias,
        slave_relative_position});
}

} // namespace

std::vector<FaultEvent> PortErrorTracker::process(
    std::uint64_t timestamp_ms,
    int slave_position,
    const std::string& slave_name,
    const PortErrorCounters& current)
{
    return process(timestamp_ms, -1, slave_position, -1, -1,
        slave_name, current);
}

std::vector<FaultEvent> PortErrorTracker::process(
    std::uint64_t timestamp_ms,
    int master_index,
    int slave_position,
    int slave_alias,
    int slave_relative_position,
    const std::string& slave_name,
    const PortErrorCounters& current)
{
    std::vector<FaultEvent> events;

    const std::string key = slave_alias > 0
        ? std::to_string(slave_alias) + ":" +
            std::to_string(slave_relative_position)
        : "position:" + std::to_string(slave_position);
    const auto found = baselines_.find(key);
    if (found == baselines_.end() ||
        found->second.slave_name != slave_name)
    {
        baselines_[key] = {slave_name, current};
        return events;
    }

    const PortErrorCounters& previous = found->second.counters;
    if (anyCounterDecreased(previous, current))
    {
        found->second.counters = current;
        return events;
    }

    for (std::size_t port = 0U;
         port < current.ports.size();
         ++port)
    {
        const PortErrorCounter& old_value = previous.ports[port];
        const PortErrorCounter& new_value = current.ports[port];
        const int port_position = static_cast<int>(port);

        appendEventIfIncreased(
            events, timestamp_ms, slave_position, master_index,
            slave_alias, slave_relative_position, port_position,
            EventType::PORT_INVALID_FRAME_INCREASED,
            old_value.invalid_frame, new_value.invalid_frame,
            "invalid frame counter");
        appendEventIfIncreased(
            events, timestamp_ms, slave_position, master_index,
            slave_alias, slave_relative_position, port_position,
            EventType::PORT_RX_ERROR_INCREASED,
            old_value.rx_error, new_value.rx_error,
            "RX error counter");
        appendEventIfIncreased(
            events, timestamp_ms, slave_position, master_index,
            slave_alias, slave_relative_position, port_position,
            EventType::PORT_FORWARDED_RX_ERROR_INCREASED,
            old_value.forwarded_rx_error,
            new_value.forwarded_rx_error,
            "forwarded RX error counter");
        appendEventIfIncreased(
            events, timestamp_ms, slave_position, master_index,
            slave_alias, slave_relative_position, port_position,
            EventType::PORT_LOST_LINK_INCREASED,
            old_value.lost_link, new_value.lost_link,
            "lost link counter");
    }

    found->second.counters = current;
    return events;
}
