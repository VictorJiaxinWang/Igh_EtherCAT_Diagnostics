#pragma once

#include "ethercat_diag/common/diag_types.h"
#include "ethercat_diag/esc/al_status_decoder.h"
#include "ethercat_diag/esc/port_error_decoder.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class PortErrorEvidenceKind
{
    INVALID_FRAME,
    RX_ERROR,
    FORWARDED_RX_ERROR,
    LOST_LINK
};

struct MasterLinkEvidence
{
    bool link_up{};
};

struct BoundaryEvidence
{
    int last_alive_slave{-1};
    int first_lost_slave{-1};
    int last_alive_alias{-1};
    int last_alive_relative_position{-1};
    int first_lost_alias{-1};
    int first_lost_relative_position{-1};
};

struct SlaveLossEvidence
{
    int slave_position{-1};
};

struct PortErrorEvidence
{
    PortErrorEvidenceKind kind{
        PortErrorEvidenceKind::INVALID_FRAME};
    int slave_position{-1};
    int port_position{-1};
    int old_value{};
    int new_value{};
    int delta{};
};

struct SlaveStateEvidence
{
    int slave_position{-1};
    AlState state{AlState::UNKNOWN};
    bool has_error{};
    bool online{};
};

struct AlEvidence
{
    int slave_position{-1};
    std::optional<std::uint16_t> raw_status;
    std::optional<EscAlState> state;
    std::optional<bool> error_indication;
    std::optional<bool> warning_indication;
    std::optional<std::uint16_t> status_code;
};

struct PortCounterSnapshotEvidence
{
    int slave_position{-1};
    PortErrorCounters counters;
};

struct DiagnosisEvidence
{
    std::uint64_t timestamp_ms{};
    std::optional<MasterLinkEvidence> master_link;
    std::optional<BoundaryEvidence> boundary;
    std::vector<SlaveLossEvidence> lost_slaves;
    std::vector<PortErrorEvidence> port_errors;
    std::vector<SlaveStateEvidence> slave_states;
    std::optional<AlEvidence> al_status;
    std::optional<PortCounterSnapshotEvidence> port_snapshot;
    std::vector<std::string> collection_errors;
};
