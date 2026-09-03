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
        slave.online = true;

        snapshot.slaves.push_back(slave);
    }

    snapshot.master.slave_count =
        static_cast<int>(
            snapshot.slaves.size());

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

int main()
{
    testLocatesTailDisconnectionBoundary();
    testReturnsInvalidWhenNoSlaveIsLost();
    testLocatesBoundaryWhenAllSlavesAreLost();
    testIgnoresVectorOrder();
    testLocatesMissingMiddlePosition();

    std::cout
        << "All BoundaryLocator tests passed\n";

    return 0;
}
