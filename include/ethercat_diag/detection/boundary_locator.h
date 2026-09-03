#pragma once

#include "ethercat_diag/common/diag_types.h"

struct FaultBoundary
{
    int last_alive_slave{-1};
    int first_lost_slave{-1};
    bool valid{false};
};

class BoundaryLocator
{
public:
    FaultBoundary locate(
        const NetworkSnapshot& previous,
        const NetworkSnapshot& current) const;
};
