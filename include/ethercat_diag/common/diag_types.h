#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class AlState
{
    INIT,
    PREOP,
    SAFEOP,
    OP,
    UNKNOWN
};

struct MasterSnapshot
{
    std::string phase;
    bool active{};
    bool link_up{};
    int slave_count{};

    std::uint64_t timestamp_ms{};
    int master_index{};
};

struct SlaveSnapshot
{
    int position;
    int alias;
    int relative_position;
    AlState state;
    bool has_error;
    bool online;
    std::string name;
};

struct SlaveIdentity
{
    int alias{};
    int relative_position{};

    bool valid() const { return alias > 0; }
};

inline SlaveIdentity stableIdentity(const SlaveSnapshot& slave)
{
    return {slave.alias, slave.relative_position};
}

inline bool sameStableIdentity(
    const SlaveSnapshot& left,
    const SlaveSnapshot& right)
{
    const SlaveIdentity a = stableIdentity(left);
    const SlaveIdentity b = stableIdentity(right);
    return a.valid() && b.valid() &&
        a.alias == b.alias &&
        a.relative_position == b.relative_position;
}

struct NetworkSnapshot
{
    MasterSnapshot master;
    std::vector<SlaveSnapshot> slaves;
};

enum class EventType
{
    MASTER_LINK_DOWN,
    MASTER_LINK_UP,
    SLAVE_COUNT_CHANGED,
    SLAVE_LOST,
    SLAVE_STATE_CHANGED,
    PORT_INVALID_FRAME_INCREASED,
    PORT_RX_ERROR_INCREASED,
    PORT_FORWARDED_RX_ERROR_INCREASED,
    PORT_LOST_LINK_INCREASED
};

struct FaultEvent
{
    std::uint64_t timestamp_ms;
    EventType type;
    int slave_position;
    int old_value;
    int new_value;
    std::string description;
    int port_position{-1};
    int master_index{-1};
    int slave_alias{-1};
    int slave_relative_position{-1};
};

struct RecoveryEvent
{
    std::uint64_t timestamp_ms{};
    std::uint64_t fault_started_ms{};
    std::uint64_t duration_ms{};
    int fault_slave_count{};
    int recovered_slave_count{};
    std::vector<int> recovered_slave_positions;
    std::vector<SlaveIdentity> recovered_slave_identities;
    std::string description;
};
