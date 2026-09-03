#pragma once

#include "ethercat_diag/common/diag_types.h"
#include "ethercat_diag/detection/boundary_locator.h"
#include "ethercat_diag/diagnosis/active_diagnosis.h"

#include <optional>
#include <cstdint>
#include <vector>

class DiagnosisCoordinator
{
public:
    explicit DiagnosisCoordinator(
        int master_index = 0,
        std::uint64_t cooldown_ms = 10000);

    DiagnosisCoordinator(
        ActiveDiagnosis diagnosis,
        std::uint64_t cooldown_ms = 10000);

    std::optional<DiagResult> process(
        const std::optional<NetworkSnapshot>& previous,
        const NetworkSnapshot& current,
        const std::vector<FaultEvent>& events);

    void resetCooldown();

private:
    BoundaryLocator boundary_locator_;
    ActiveDiagnosis diagnosis_;
    std::uint64_t cooldown_ms_{};
    std::optional<std::uint64_t> last_diagnosis_ms_;
};
