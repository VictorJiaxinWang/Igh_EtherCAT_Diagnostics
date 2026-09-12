#pragma once

#include "ethercat_diag/common/diag_types.h"

struct FaultBoundary
{
    int last_alive_slave{-1};
    int first_lost_slave{-1};
    bool valid{false};
    int last_alive_alias{-1};
    int last_alive_relative_position{-1};
    int first_lost_alias{-1};
    int first_lost_relative_position{-1};
};

class BoundaryLocator
{
public:
    FaultBoundary locate(
        const NetworkSnapshot& previous,
        const NetworkSnapshot& current) const;
};
