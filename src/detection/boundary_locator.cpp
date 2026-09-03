#include "ethercat_diag/detection/boundary_locator.h"
#include <optional>
#include <set>


FaultBoundary BoundaryLocator::locate(
    const NetworkSnapshot& previous,
    const NetworkSnapshot& current) const
{
    FaultBoundary boundary;

    std::set<int> current_positions;

    for (const SlaveSnapshot& slave :
         current.slaves)
    {
        current_positions.insert(
            slave.position);
    }

    std::optional<int> first_lost;

    for (const SlaveSnapshot& old_slave :
         previous.slaves)
    {
        const bool still_exists =
            current_positions.count(
                old_slave.position) != 0;

        if (still_exists)
        {
            continue;
        }

        if (!first_lost ||
            old_slave.position < *first_lost)
        {
            first_lost =
                old_slave.position;
        }
    }

    if (!first_lost)
    {
        return boundary;
    }

    int last_alive = -1;

    for (const SlaveSnapshot& current_slave :
         current.slaves)
    {
        if (current_slave.position <
                *first_lost &&
            current_slave.position >
                last_alive)
        {
            last_alive =
                current_slave.position;
        }
    }

    boundary.last_alive_slave =
        last_alive;

    boundary.first_lost_slave =
        *first_lost;

    boundary.valid = true;

    return boundary;
}