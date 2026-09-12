#include "ethercat_diag/detection/recovery_tracker.h"

#include <algorithm>

namespace
{
bool sameIdentity(const SlaveIdentity& a, const SlaveIdentity& b)
{
    return a.valid() && b.valid() && a.alias == b.alias &&
        a.relative_position == b.relative_position;
}

bool containsIdentity(const NetworkSnapshot& snapshot, const SlaveIdentity& id)
{
    return std::any_of(snapshot.slaves.begin(), snapshot.slaves.end(),
        [&id](const SlaveSnapshot& slave) {
            return sameIdentity(stableIdentity(slave), id);
        });
}

bool isFaultTrigger(const FaultEvent& event)
{
    return event.type == EventType::MASTER_LINK_DOWN ||
        event.type == EventType::SLAVE_LOST ||
        (event.type == EventType::SLAVE_COUNT_CHANGED &&
         event.new_value < event.old_value);
}
} // namespace

void RecoveryTracker::appendMissingIdentities(
    FaultEpisode& episode, const NetworkSnapshot& current)
{
    for (const SlaveIdentity& id : episode.expected_identities)
    {
        if (containsIdentity(current, id))
        {
            continue;
        }
        if (std::none_of(episode.missing_identities.begin(),
                episode.missing_identities.end(),
                [&id](const SlaveIdentity& value) {
                    return sameIdentity(value, id);
                }))
        {
            episode.missing_identities.push_back(id);
        }
    }
}

bool RecoveryTracker::topologyRecovered(
    const FaultEpisode& episode, const NetworkSnapshot& current)
{
    return current.master.link_up &&
        current.master.slave_count >= episode.expected_slave_count &&
        std::all_of(episode.expected_identities.begin(),
            episode.expected_identities.end(),
            [&current](const SlaveIdentity& id) {
                return containsIdentity(current, id);
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
        const auto trigger = std::find_if(events.begin(), events.end(), isFaultTrigger);
        if (trigger == events.end() || !previous)
        {
            return std::nullopt;
        }
        FaultEpisode episode;
        episode.started_ms = trigger->timestamp_ms;
        episode.expected_slave_count = previous->master.slave_count;
        episode.minimum_slave_count = current.master.slave_count;
        for (const SlaveSnapshot& slave : previous->slaves)
        {
            const SlaveIdentity id = stableIdentity(slave);
            if (!id.valid())
            {
                return std::nullopt;
            }
            episode.expected_identities.push_back(id);
        }
        episode_ = std::move(episode);
        started_now = true;
    }

    episode_->minimum_slave_count = std::min(
        episode_->minimum_slave_count, current.master.slave_count);
    appendMissingIdentities(*episode_, current);
    if (started_now || !topologyRecovered(*episode_, current))
    {
        return std::nullopt;
    }

    RecoveryEvent recovery;
    recovery.timestamp_ms = current.master.timestamp_ms;
    recovery.fault_started_ms = episode_->started_ms;
    recovery.duration_ms = recovery.timestamp_ms >= recovery.fault_started_ms
        ? recovery.timestamp_ms - recovery.fault_started_ms : 0U;
    recovery.fault_slave_count = episode_->minimum_slave_count;
    recovery.recovered_slave_count = current.master.slave_count;
    recovery.recovered_slave_identities = episode_->missing_identities;
    for (const SlaveIdentity& id : episode_->missing_identities)
    {
        const auto found = std::find_if(current.slaves.begin(), current.slaves.end(),
            [&id](const SlaveSnapshot& slave) {
                return sameIdentity(stableIdentity(slave), id);
            });
        recovery.recovered_slave_positions.push_back(
            found == current.slaves.end() ? -1 : found->position);
    }
    recovery.description = "EtherCAT network recovered";
    episode_.reset();
    return recovery;
}

bool RecoveryTracker::faultActive() const
{
    return episode_.has_value();
}
