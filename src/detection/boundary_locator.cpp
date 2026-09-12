#include "ethercat_diag/detection/boundary_locator.h"

#include <algorithm>
#include <vector>

FaultBoundary BoundaryLocator::locate(
    const NetworkSnapshot& previous,
    const NetworkSnapshot& current) const
{
    FaultBoundary boundary;
    const auto currentMatch = [&current](const SlaveSnapshot& wanted) {
        return std::find_if(
            current.slaves.begin(), current.slaves.end(),
            [&wanted](const SlaveSnapshot& candidate) {
                return sameStableIdentity(wanted, candidate);
            });
    };

    std::vector<const SlaveSnapshot*> baseline;
    for (const SlaveSnapshot& slave : previous.slaves)
    {
        baseline.push_back(&slave);
    }
    std::sort(baseline.begin(), baseline.end(),
        [](const SlaveSnapshot* left, const SlaveSnapshot* right) {
            return left->position < right->position;
        });

    auto first_lost = baseline.end();
    for (auto it = baseline.begin(); it != baseline.end(); ++it)
    {
        if (!stableIdentity(**it).valid())
        {
            return boundary;
        }
        if (currentMatch(**it) == current.slaves.end())
        {
            first_lost = it;
            break;
        }
    }
    if (first_lost == baseline.end())
    {
        return boundary;
    }

    boundary.first_lost_slave = (*first_lost)->position;
    boundary.first_lost_alias = (*first_lost)->alias;
    boundary.first_lost_relative_position = (*first_lost)->relative_position;
    for (auto it = first_lost; it != baseline.begin();)
    {
        --it;
        const auto alive = currentMatch(**it);
        if (alive != current.slaves.end())
        {
            boundary.last_alive_slave = alive->position;
            boundary.last_alive_alias = alive->alias;
            boundary.last_alive_relative_position = alive->relative_position;
            break;
        }
    }
    boundary.valid = true;
    return boundary;
}
