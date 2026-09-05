#pragma once

#include "ethercat_diag/diagnosis/active_diagnosis.h"
#include "ethercat_diag/root_cause/diagnosis_evidence.h"

#include <optional>
#include <vector>

class EvidenceBuilder
{
public:
    DiagnosisEvidence build(
        const NetworkSnapshot& snapshot,
        const std::vector<FaultEvent>& events,
        const std::optional<DiagResult>& diagnosis) const;
};
