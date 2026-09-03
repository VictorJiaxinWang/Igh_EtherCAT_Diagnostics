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
    SLAVE_STATE_CHANGED
};

struct FaultEvent
{
    std::uint64_t timestamp_ms;
    EventType type;
    int slave_position;
    int old_value;
    int new_value;
    std::string description;
};

struct RecoveryEvent
{
    std::uint64_t timestamp_ms{};
    std::uint64_t fault_started_ms{};
    std::uint64_t duration_ms{};
    int fault_slave_count{};
    int recovered_slave_count{};
    std::vector<int> recovered_slave_positions;
    std::string description;
};
