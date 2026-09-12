#include "ethercat_diag/detection/boundary_locator.h"

#include <cassert>
#include <iostream>
#include <vector>

NetworkSnapshot makeSnapshotWithPositions(
    const std::vector<int>& positions)
{
    NetworkSnapshot snapshot;

    for (const int position : positions)
    {
        SlaveSnapshot slave{};

        slave.position = position;
        slave.alias = position + 1;
        slave.online = true;

        snapshot.slaves.push_back(slave);
    }

    snapshot.master.slave_count =
        static_cast<int>(
            snapshot.slaves.size());

    return snapshot;
}

NetworkSnapshot makeSnapshotWithAliases(
    const std::vector<std::pair<int, int>>& position_aliases)
{
    NetworkSnapshot snapshot;
    for (const auto& [position, alias] : position_aliases)
    {
        SlaveSnapshot slave{};
        slave.position = position;
        slave.alias = alias;
        slave.online = true;
        snapshot.slaves.push_back(slave);
    }
    snapshot.master.slave_count = static_cast<int>(snapshot.slaves.size());
    return snapshot;
}

void testLocatesTailDisconnectionBoundary()
{
    const NetworkSnapshot previous =
        makeSnapshotWithPositions(
            {0, 1, 2, 3, 4, 5});

    const NetworkSnapshot current =
        makeSnapshotWithPositions(
            {0, 1, 2});

    BoundaryLocator locator;

    const FaultBoundary boundary =
        locator.locate(previous, current);

    assert(boundary.valid);
    assert(boundary.last_alive_slave == 2);
    assert(boundary.first_lost_slave == 3);
}

void testReturnsInvalidWhenNoSlaveIsLost()
{
    const NetworkSnapshot previous =
        makeSnapshotWithPositions(
            {0, 1, 2, 3});

    const NetworkSnapshot current =
        makeSnapshotWithPositions(
            {0, 1, 2, 3});

    BoundaryLocator locator;

    const FaultBoundary boundary =
        locator.locate(previous, current);

    assert(!boundary.valid);
    assert(boundary.last_alive_slave == -1);
    assert(boundary.first_lost_slave == -1);
}

void testLocatesBoundaryWhenAllSlavesAreLost()
{
    const NetworkSnapshot previous =
        makeSnapshotWithPositions(
            {0, 1, 2, 3});

    const NetworkSnapshot current =
        makeSnapshotWithPositions({});

    BoundaryLocator locator;

    const FaultBoundary boundary =
        locator.locate(previous, current);

    assert(boundary.valid);

    // -1代表Master。
    assert(boundary.last_alive_slave == -1);

    assert(boundary.first_lost_slave == 0);
}

void testIgnoresVectorOrder()
{
    const NetworkSnapshot previous =
        makeSnapshotWithPositions(
            {5, 2, 0, 4, 1, 3});

    const NetworkSnapshot current =
        makeSnapshotWithPositions(
            {2, 0, 1});

    BoundaryLocator locator;

    const FaultBoundary boundary =
        locator.locate(previous, current);

    assert(boundary.valid);
    assert(boundary.last_alive_slave == 2);
    assert(boundary.first_lost_slave == 3);
}

void testLocatesMissingMiddlePosition()
{
    const NetworkSnapshot previous =
        makeSnapshotWithPositions(
            {0, 1, 2, 3});

    const NetworkSnapshot current =
        makeSnapshotWithPositions(
            {0, 2, 3});

    BoundaryLocator locator;

    const FaultBoundary boundary =
        locator.locate(previous, current);

    assert(boundary.valid);
    assert(boundary.last_alive_slave == 0);
    assert(boundary.first_lost_slave == 1);
}

void testUsesAliasWhenRescanChangesPositions()
{
    const NetworkSnapshot previous = makeSnapshotWithAliases(
        {{0, 10}, {1, 11}, {2, 12}, {3, 13}});
    const NetworkSnapshot current = makeSnapshotWithAliases(
        {{0, 10}, {1, 12}, {2, 13}});

    const FaultBoundary boundary = BoundaryLocator{}.locate(previous, current);
    assert(boundary.valid);
    assert(boundary.last_alive_slave == 0);
    assert(boundary.last_alive_alias == 10);
    assert(boundary.first_lost_slave == 1);
    assert(boundary.first_lost_alias == 11);
}

int main()
{
    testLocatesTailDisconnectionBoundary();
    testReturnsInvalidWhenNoSlaveIsLost();
    testLocatesBoundaryWhenAllSlavesAreLost();
    testIgnoresVectorOrder();
    testLocatesMissingMiddlePosition();
    testUsesAliasWhenRescanChangesPositions();

    std::cout
        << "All BoundaryLocator tests passed\n";

    return 0;
}
