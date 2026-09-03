#pragma once

#include "ethercat_diag/common/diag_types.h"

#include <optional>
#include <vector>

class EventDetector
{
public:
    std::vector<FaultEvent> process(
        const NetworkSnapshot& current);

private:
    std::optional<NetworkSnapshot> previous_;
};
