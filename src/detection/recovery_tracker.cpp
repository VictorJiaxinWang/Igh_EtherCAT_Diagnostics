#include "ethercat_diag/detection/recovery_tracker.h"

#include <algorithm>

namespace
{

bool containsPosition(
    const NetworkSnapshot& snapshot,
    int position)
{
    return std::any_of(
        snapshot.slaves.begin(),
        snapshot.slaves.end(),
        [position](const SlaveSnapshot& slave) {
            return slave.position == position;
        });
}

bool isFaultTrigger(const FaultEvent& event)
{
    if (event.type == EventType::MASTER_LINK_DOWN ||
        event.type == EventType::SLAVE_LOST)
    {
        return true;
    }

    return event.type == EventType::SLAVE_COUNT_CHANGED &&
           event.new_value < event.old_value;
}

const FaultEvent* findFaultTrigger(
    const std::vector<FaultEvent>& events)
{
    const auto found = std::find_if(
        events.begin(),
        events.end(),
        isFaultTrigger);

    return found == events.end() ? nullptr : &*found;
}

} // namespace

void RecoveryTracker::appendMissingPositions(
    FaultEpisode& episode,
    const NetworkSnapshot& current)
{
    for (const int position : episode.expected_positions)
    {
        if (containsPosition(current, position))
        {
            continue;
        }

        if (std::find(
                episode.missing_positions.begin(),
                episode.missing_positions.end(),
                position) == episode.missing_positions.end())
        {
            episode.missing_positions.push_back(position);
        }
    }
}

bool RecoveryTracker::topologyRecovered(
    const FaultEpisode& episode,
    const NetworkSnapshot& current)
{
    if (!current.master.link_up ||
        current.master.slave_count < episode.expected_slave_count)
    {
        return false;
    }

    return std::all_of(
        episode.expected_positions.begin(),
        episode.expected_positions.end(),
        [&current](int position) {
            return containsPosition(current, position);
        });
}

std::optional<RecoveryEvent> RecoveryTracker::process(
    const std::optional<NetworkSnapshot>& previous,
    const NetworkSnapshot& current,
    const std::vector<FaultEvent>& events)
{
    bool started_now = false;

    if (!episode_)
    {
        const FaultEvent* trigger = findFaultTrigger(events);

        if (trigger == nullptr || !previous)
        {
            return std::nullopt;
        }

        FaultEpisode episode;
        episode.started_ms = trigger->timestamp_ms;
        episode.expected_slave_count =
            previous->master.slave_count;
        episode.minimum_slave_count =
            current.master.slave_count;

        for (const SlaveSnapshot& slave : previous->slaves)
        {
            episode.expected_positions.push_back(
                slave.position);
        }

        episode_ = std::move(episode);
        started_now = true;
    }

    episode_->minimum_slave_count = std::min(
        episode_->minimum_slave_count,
        current.master.slave_count);

    appendMissingPositions(*episode_, current);

    if (started_now || !topologyRecovered(*episode_, current))
    {
        return std::nullopt;
    }

    RecoveryEvent recovery;
    recovery.timestamp_ms = current.master.timestamp_ms;
    recovery.fault_started_ms = episode_->started_ms;
    recovery.duration_ms =
        recovery.timestamp_ms >= recovery.fault_started_ms
            ? recovery.timestamp_ms - recovery.fault_started_ms
            : 0;
    recovery.fault_slave_count =
        episode_->minimum_slave_count;
    recovery.recovered_slave_count =
        current.master.slave_count;
    recovery.recovered_slave_positions =
        episode_->missing_positions;
    recovery.description = "EtherCAT network recovered";

    episode_.reset();
    return recovery;
}

bool RecoveryTracker::faultActive() const
{
    return episode_.has_value();
}
