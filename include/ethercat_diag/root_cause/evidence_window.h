#pragma once

#include "ethercat_diag/root_cause/diagnosis_evidence.h"

#include <cstdint>
#include <deque>
#include <vector>

class EvidenceWindow
{
public:
    explicit EvidenceWindow(
        std::uint64_t duration_ms = 10000U);

    void push(DiagnosisEvidence evidence);

    const std::deque<DiagnosisEvidence>& recent() const noexcept;

    std::vector<DiagnosisEvidence> relatedToBoundary(
        const BoundaryEvidence& boundary) const;

private:
    std::uint64_t duration_ms_{};
    std::deque<DiagnosisEvidence> frames_;
};
