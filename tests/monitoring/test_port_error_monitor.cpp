#include "ethercat_diag/monitoring/port_error_monitor.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{

NetworkSnapshot makeSnapshot(
    std::uint64_t timestamp_ms,
    const std::vector<int>& positions)
{
    NetworkSnapshot snapshot;
    snapshot.master.timestamp_ms = timestamp_ms;
    snapshot.master.master_index = 0;

    for (const int position : positions)
    {
        snapshot.slaves.push_back(SlaveSnapshot{
            position,
            0,
            position,
            AlState::OP,
            false,
            true,
            "slave-" + std::to_string(position)});
    }

    return snapshot;
}

PortErrorReadResult successfulRead(std::uint8_t value)
{
    PortErrorCounters counters;
    counters.ports[0].lost_link = value;
    return {true, counters, {}};
}

void testReadsOneOnlineSlavePerCycleInRoundRobinOrder()
{
    PortErrorMonitor monitor(
        0,
        [](int, int slave_position) {
            return successfulRead(
                static_cast<std::uint8_t>(slave_position));
        });
    const NetworkSnapshot snapshot =
        makeSnapshot(1000U, {0, 1, 2});

    const auto first = monitor.process(snapshot);
    const auto second = monitor.process(snapshot);
    const auto third = monitor.process(snapshot);
    const auto fourth = monitor.process(snapshot);

    assert(first.attempted && first.slave_position == 0);
    assert(second.attempted && second.slave_position == 1);
    assert(third.attempted && third.slave_position == 2);
    assert(fourth.attempted && fourth.slave_position == 0);
}

void testCounterIncreaseBecomesAnEvent()
{
    std::uint8_t value = 4U;
    PortErrorMonitor monitor(
        0,
        [&value](int, int) {
            return successfulRead(value);
        });
    NetworkSnapshot snapshot = makeSnapshot(1000U, {3});

    const auto baseline = monitor.process(snapshot);
    value = 5U;
    snapshot.master.timestamp_ms = 2000U;
    const auto changed = monitor.process(snapshot);

    assert(baseline.events.empty());
    assert(changed.events.size() == 1U);
    assert(changed.events[0].type ==
           EventType::PORT_LOST_LINK_INCREASED);
    assert(changed.events[0].slave_position == 3);
    assert(changed.events[0].port_position == 0);
}

void testReadFailureIsReportedAndDoesNotStopRoundRobin()
{
    PortErrorMonitor monitor(
        0,
        [](int, int slave_position) {
            if (slave_position == 0)
            {
                return PortErrorReadResult{
                    false, {}, "temporary read failure"};
            }
            return successfulRead(0U);
        });
    const NetworkSnapshot snapshot =
        makeSnapshot(1000U, {0, 1});

    const auto failed = monitor.process(snapshot);
    const auto next = monitor.process(snapshot);

    assert(failed.attempted);
    assert(failed.slave_position == 0);
    assert(failed.error == "temporary read failure");
    assert(failed.events.empty());
    assert(next.attempted);
    assert(next.slave_position == 1);
    assert(next.error.empty());
}

void testNoOnlineSlaveMeansNoReadAttempt()
{
    int read_count = 0;
    PortErrorMonitor monitor(
        0,
        [&read_count](int, int) {
            ++read_count;
            return successfulRead(0U);
        });

    const auto result = monitor.process(makeSnapshot(1000U, {}));

    assert(!result.attempted);
    assert(result.events.empty());
    assert(read_count == 0);
}

} // namespace

int main()
{
    testReadsOneOnlineSlavePerCycleInRoundRobinOrder();
    testCounterIncreaseBecomesAnEvent();
    testReadFailureIsReportedAndDoesNotStopRoundRobin();
    testNoOnlineSlaveMeansNoReadAttempt();
    std::cout << "port error monitor tests passed\n";
}
