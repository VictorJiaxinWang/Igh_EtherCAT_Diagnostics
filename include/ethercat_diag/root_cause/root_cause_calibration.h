#pragma once

#include "ethercat_diag/root_cause/root_cause_analyzer.h"

#include <cstddef>
#include <string>
#include <vector>

struct FaultMatrixCase
{
    std::string name;
    RootCauseKind expected{RootCauseKind::UNKNOWN};
    std::vector<DiagnosisEvidence> evidence;
};

struct FaultMatrixEntry
{
    std::string name;
    RootCauseKind expected{RootCauseKind::UNKNOWN};
    RootCauseKind actual{RootCauseKind::UNKNOWN};
    int score{};
    double confidence{};
    bool conclusive{};
    bool passed{};
};

struct FaultMatrixSummary
{
    std::vector<FaultMatrixEntry> entries;
    std::size_t passed{};

    double accuracy() const noexcept;
};

class RootCauseCalibration
{
public:
    explicit RootCauseCalibration(
        RootCauseAnalyzer analyzer = RootCauseAnalyzer{});

    FaultMatrixSummary evaluate(
        const std::vector<FaultMatrixCase>& cases) const;

private:
    RootCauseAnalyzer analyzer_;
};

std::string formatFaultMatrixSummary(
    const FaultMatrixSummary& summary);
