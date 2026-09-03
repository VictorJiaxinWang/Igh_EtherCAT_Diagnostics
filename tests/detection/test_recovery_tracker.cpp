#include "ethercat_diag/detection/recovery_tracker.h"

#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace
{

void require(bool condition, std::string_view message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

SlaveSnapshot makeSlave(int position)
{
    return {
        position,
        1000,
        position,
        AlState::OP,
        false,
        true,
        "Test slave"
    };
}

NetworkSnapshot makeSnapshot(
    std::uint64_t timestamp_ms,
    bool link_up,
    std::initializer_list<int> positions)
{
    NetworkSnapshot snapshot;
    snapshot.master.timestamp_ms = timestamp_ms;
    snapshot.master.link_up = link_up;
    snapshot.master.slave_count =
        static_cast<int>(positions.size());

    for (const int position : positions)
    {
        snapshot.slaves.push_back(makeSlave(position));
    }

    return snapshot;
}

FaultEvent makeEvent(
    std::uint64_t timestamp_ms,
    EventType type,
    int old_value,
    int new_value)
{
    return {
        timestamp_ms,
        type,
        -1,
        old_value,
        new_value,
        "Test event"
    };
}

void testHealthySamplesDoNotCreateRecovery()
{
    RecoveryTracker tracker;
    const NetworkSnapshot previous =
        makeSnapshot(1000, true, {0, 1, 2, 3});
    const NetworkSnapshot current =
        makeSnapshot(2000, true, {0, 1, 2, 3});

    const auto recovery =
        tracker.process(previous, current, {});

    require(!recovery.has_value(),
            "healthy samples do not create recovery");
    require(!tracker.faultActive(),
            "healthy samples do not open a fault episode");
}

void testPartialReturnDoesNotCreateRecovery()
{
    RecoveryTracker tracker;
    const NetworkSnapshot healthy =
        makeSnapshot(1000, true, {0, 1, 2, 3});
    const NetworkSnapshot fault =
        makeSnapshot(2000, true, {0, 1});

    tracker.process(
        healthy,
        fault,
        {makeEvent(2000,
                   EventType::SLAVE_COUNT_CHANGED,
                   4,
                   2)});

    const auto recovery = tracker.process(
        fault,
        makeSnapshot(3000, true, {0, 1, 2}),
        {makeEvent(3000,
                   EventType::SLAVE_COUNT_CHANGED,
                   2,
                   3)});

    require(!recovery.has_value(),
            "partial slave return is not full recovery");
    require(tracker.faultActive(),
            "partial return keeps the fault episode active");
}

void testFullReturnCreatesOneRecoveryWithDuration()
{
    RecoveryTracker tracker;
    const NetworkSnapshot healthy =
        makeSnapshot(1000, true, {0, 1, 2, 3});
    const NetworkSnapshot fault =
        makeSnapshot(2000, true, {0, 1});

    tracker.process(
        healthy,
        fault,
        {makeEvent(2000,
                   EventType::SLAVE_COUNT_CHANGED,
                   4,
                   2)});

    const NetworkSnapshot recovered =
        makeSnapshot(5200, true, {0, 1, 2, 3});

    const auto recovery = tracker.process(
        fault,
        recovered,
        {makeEvent(5200,
                   EventType::SLAVE_COUNT_CHANGED,
                   2,
                   4)});

    require(recovery.has_value(),
            "full topology return creates recovery");
    require(recovery->timestamp_ms == 5200,
            "recovery timestamp comes from current snapshot");
    require(recovery->fault_started_ms == 2000,
            "fault start timestamp is preserved");
    require(recovery->duration_ms == 3200,
            "fault duration is calculated in milliseconds");
    require(recovery->fault_slave_count == 2,
            "lowest observed slave count is recorded");
    require(recovery->recovered_slave_count == 4,
            "recovered slave count is recorded");
    require(recovery->recovered_slave_positions ==
                std::vector<int>({2, 3}),
            "returned slave positions are recorded");
    require(!tracker.faultActive(),
            "recovery closes the fault episode");

    const auto duplicate = tracker.process(
        recovered,
        makeSnapshot(6200, true, {0, 1, 2, 3}),
        {});

    require(!duplicate.has_value(),
            "continued health does not duplicate recovery");
}

void testLinkMustRecoverAlongWithTopology()
{
    RecoveryTracker tracker;
    const NetworkSnapshot healthy =
        makeSnapshot(1000, true, {0, 1});
    const NetworkSnapshot link_down =
        makeSnapshot(2000, false, {0, 1});

    tracker.process(
        healthy,
        link_down,
        {makeEvent(2000,
                   EventType::MASTER_LINK_DOWN,
                   1,
                   0)});

    const auto still_down = tracker.process(
        link_down,
        makeSnapshot(3000, false, {0, 1}),
        {});

    require(!still_down.has_value(),
            "complete topology with down link is not recovery");

    const auto recovered = tracker.process(
        link_down,
        makeSnapshot(4000, true, {0, 1}),
        {makeEvent(4000,
                   EventType::MASTER_LINK_UP,
                   0,
                   1)});

    require(recovered.has_value(),
            "link restoration completes link fault episode");
    require(recovered->duration_ms == 2000,
            "link fault duration is recorded");
}

void testClockRollbackDoesNotUnderflowDuration()
{
    RecoveryTracker tracker;
    const NetworkSnapshot healthy =
        makeSnapshot(4000, true, {0, 1});
    const NetworkSnapshot fault =
        makeSnapshot(5000, true, {0});

    tracker.process(
        healthy,
        fault,
        {makeEvent(5000,
                   EventType::SLAVE_COUNT_CHANGED,
                   2,
                   1)});

    const auto recovery = tracker.process(
        fault,
        makeSnapshot(3000, true, {0, 1}),
        {makeEvent(3000,
                   EventType::SLAVE_COUNT_CHANGED,
                   1,
                   2)});

    require(recovery.has_value(),
            "topology recovery survives clock rollback");
    require(recovery->duration_ms == 0,
            "clock rollback clamps duration to zero");
}

} // namespace

int main()
{
    testHealthySamplesDoNotCreateRecovery();
    testPartialReturnDoesNotCreateRecovery();
    testFullReturnCreatesOneRecoveryWithDuration();
    testLinkMustRecoverAlongWithTopology();
    testClockRollbackDoesNotUnderflowDuration();

    std::cout << "All recovery tracker tests passed\n";
    return 0;
}
