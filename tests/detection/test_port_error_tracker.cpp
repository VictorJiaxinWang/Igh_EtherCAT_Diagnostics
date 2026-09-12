#include "ethercat_diag/detection/port_error_tracker.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace
{

PortErrorCounters countersWithPort0(
    std::uint8_t invalid_frame,
    std::uint8_t rx_error,
    std::uint8_t forwarded_rx_error,
    std::uint8_t lost_link)
{
    PortErrorCounters counters;
    counters.ports[0] = {
        invalid_frame,
        rx_error,
        forwarded_rx_error,
        lost_link};
    return counters;
}

void testFirstObservationOnlyEstablishesBaseline()
{
    PortErrorTracker tracker;

    const std::vector<FaultEvent> events = tracker.process(
        1000U,
        3,
        "drive",
        countersWithPort0(4U, 5U, 6U, 7U));

    assert(events.empty());
}

void testEmitsOneTypedEventForEachIncreasingCounter()
{
    PortErrorTracker tracker;
    tracker.process(
        1000U, 3, "drive",
        countersWithPort0(1U, 2U, 3U, 4U));

    const std::vector<FaultEvent> events = tracker.process(
        2000U, 3, "drive",
        countersWithPort0(2U, 4U, 6U, 8U));

    assert(events.size() == 4U);
    assert(events[0].type == EventType::PORT_INVALID_FRAME_INCREASED);
    assert(events[1].type == EventType::PORT_RX_ERROR_INCREASED);
    assert(events[2].type == EventType::PORT_FORWARDED_RX_ERROR_INCREASED);
    assert(events[3].type == EventType::PORT_LOST_LINK_INCREASED);

    for (const FaultEvent& event : events)
    {
        assert(event.timestamp_ms == 2000U);
        assert(event.slave_position == 3);
        assert(event.port_position == 0);
        assert(!event.description.empty());
    }

    assert(events[0].old_value == 1);
    assert(events[0].new_value == 2);
    assert(events[3].old_value == 4);
    assert(events[3].new_value == 8);
}

void testUnchangedAndSaturatedCountersDoNotRepeatEvents()
{
    PortErrorTracker tracker;
    tracker.process(
        1000U, 1, "io",
        countersWithPort0(254U, 0U, 0U, 0U));

    const auto increased = tracker.process(
        2000U, 1, "io",
        countersWithPort0(255U, 0U, 0U, 0U));
    const auto unchanged = tracker.process(
        3000U, 1, "io",
        countersWithPort0(255U, 0U, 0U, 0U));

    assert(increased.size() == 1U);
    assert(unchanged.empty());
}

void testAnyDecreaseRebaselinesTheWholeEscWithoutFalseEvent()
{
    PortErrorTracker tracker;
    tracker.process(
        1000U, 2, "drive",
        countersWithPort0(20U, 20U, 0U, 0U));

    const auto reset = tracker.process(
        2000U, 2, "drive",
        countersWithPort0(0U, 21U, 0U, 0U));
    const auto after_reset = tracker.process(
        3000U, 2, "drive",
        countersWithPort0(1U, 22U, 0U, 0U));

    assert(reset.empty());
    assert(after_reset.size() == 2U);
    assert(after_reset[0].old_value == 0);
    assert(after_reset[1].old_value == 21);
}

void testChangedSlaveIdentityEstablishesANewBaseline()
{
    PortErrorTracker tracker;
    tracker.process(
        1000U, 2, "old device",
        countersWithPort0(10U, 0U, 0U, 0U));

    const auto replacement = tracker.process(
        2000U, 2, "new device",
        countersWithPort0(20U, 0U, 0U, 0U));

    assert(replacement.empty());
}

void testAliasKeepsBaselineAcrossPositionChange()
{
    PortErrorTracker tracker;
    tracker.process(1000U, 0, 6, 15, 0, "drive",
        countersWithPort0(1U, 0U, 0U, 0U));
    const auto events = tracker.process(2000U, 0, 5, 15, 0, "drive",
        countersWithPort0(2U, 0U, 0U, 0U));
    assert(events.size() == 1U);
    assert(events[0].slave_position == 5);
    assert(events[0].slave_alias == 15);
    assert(events[0].master_index == 0);
}

} // namespace

int main()
{
    testFirstObservationOnlyEstablishesBaseline();
    testEmitsOneTypedEventForEachIncreasingCounter();
    testUnchangedAndSaturatedCountersDoNotRepeatEvents();
    testAnyDecreaseRebaselinesTheWholeEscWithoutFalseEvent();
    testChangedSlaveIdentityEstablishesANewBaseline();
    testAliasKeepsBaselineAcrossPositionChange();
    std::cout << "port error tracker tests passed\n";
}
