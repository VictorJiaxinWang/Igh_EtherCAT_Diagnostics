#include "ethercat_diag/diagnosis/diagnosis_coordinator.h"

#include <utility>

DiagnosisCoordinator::DiagnosisCoordinator(
    int master_index,
    std::uint64_t cooldown_ms)
    : diagnosis_(master_index),
      cooldown_ms_(cooldown_ms)
{
}

DiagnosisCoordinator::DiagnosisCoordinator(
    ActiveDiagnosis diagnosis,
    std::uint64_t cooldown_ms)
    : diagnosis_(std::move(diagnosis)),
      cooldown_ms_(cooldown_ms)
{
}

std::optional<DiagResult> DiagnosisCoordinator::process(
    const std::optional<NetworkSnapshot>& previous,
    const NetworkSnapshot& current,
    const std::vector<FaultEvent>& events)
{
    DiagResult result;
    FaultBoundary boundary;

    for (const FaultEvent& event: events)
    {
        if (event.type == EventType::SLAVE_LOST)
        {
            if (last_diagnosis_ms_ &&
                event.timestamp_ms >= *last_diagnosis_ms_ &&
                event.timestamp_ms - *last_diagnosis_ms_ <
                    cooldown_ms_)
            {
                return std::nullopt;
            }

            if (previous)
            {
                boundary = boundary_locator_.locate(*previous, current);
                result = diagnosis_.run(boundary);
            }
            else
            {
                result = diagnosis_.run(FaultBoundary{});
            }

            if (result.attempted())
            {
                last_diagnosis_ms_ = event.timestamp_ms;
            }

            return result;
        }
    }

    return std::nullopt;
}

void DiagnosisCoordinator::resetCooldown()
{
    last_diagnosis_ms_.reset();
}
