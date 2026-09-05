#pragma once

#include "ethercat_diag/root_cause/diagnosis_evidence.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class RootCauseKind
{
    MASTER_LINK_FAILURE,
    BOUNDARY_LINK_FAILURE,
    SLAVE_INTERNAL_ERROR,
    LINK_QUALITY_DEGRADATION,
    UNKNOWN
};

struct RootCauseCandidate
{
    RootCauseKind kind{RootCauseKind::UNKNOWN};
    int score{};
    std::vector<std::string> supporting_evidence;
    std::vector<std::string> contradicting_evidence;
    std::vector<std::string> recommended_actions;
};

struct RootCauseReport
{
    std::uint64_t timestamp_ms{};
    std::optional<BoundaryEvidence> boundary;
    RootCauseCandidate primary;
    std::vector<RootCauseCandidate> candidates;
    double confidence{};
    bool conclusive{};
    std::vector<std::string> collection_errors;
};
