#pragma once

#include "ethercat_diag/common/diag_types.h"

#include <cstdint>
#include <optional>
#include <vector>

class RecoveryTracker
{
public:
    std::optional<RecoveryEvent> process(
        const std::optional<NetworkSnapshot>& previous,
        const NetworkSnapshot& current,
        const std::vector<FaultEvent>& events);

    bool faultActive() const;

private:
    struct FaultEpisode
    {
        std::uint64_t started_ms{};
        int expected_slave_count{};
        int minimum_slave_count{};
        std::vector<SlaveIdentity> expected_identities;
        std::vector<SlaveIdentity> missing_identities;
    };

    static void appendMissingIdentities(
        FaultEpisode& episode,
        const NetworkSnapshot& current);

    static bool topologyRecovered(
        const FaultEpisode& episode,
        const NetworkSnapshot& current);

    std::optional<FaultEpisode> episode_;
};
