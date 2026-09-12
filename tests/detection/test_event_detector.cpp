#include "ethercat_diag/detection/event_detector.h"

#include <cassert>
#include <cstdint>
#include <iostream>

const FaultEvent* findEvent(
    const std::vector<FaultEvent>& events,
    EventType type,
    int slave_position);

NetworkSnapshot makeSnapshot(
    bool link_up,
    int slave_count,
    std::uint64_t timestamp_ms)
{
    NetworkSnapshot snapshot;

    snapshot.master.link_up = link_up;
    snapshot.master.slave_count = slave_count;
    snapshot.master.timestamp_ms = timestamp_ms;

    return snapshot;
}

SlaveSnapshot makeSlave(
    int position,
    AlState state = AlState::PREOP)
{
    SlaveSnapshot slave{};

    slave.position = position;
    slave.alias = position + 1;
    slave.state = state;
    slave.online = true;

    return slave;
}

void testRescanPositionChangeDoesNotCreateFalseEvents()
{
    EventDetector detector;
    NetworkSnapshot previous = makeSnapshot(true, 3, 1000);
    previous.slaves = {makeSlave(0), makeSlave(1), makeSlave(2)};

    NetworkSnapshot current = makeSnapshot(true, 2, 2000);
    current.slaves = {makeSlave(0), makeSlave(1)};
    current.slaves[1].alias = 3; // old position 2 moved to position 1

    detector.process(previous);
    const auto events = detector.process(current);
    const FaultEvent* lost = findEvent(events, EventType::SLAVE_LOST, 1);
    assert(lost != nullptr);
    assert(lost->slave_alias == 2);
    assert(findEvent(events, EventType::SLAVE_LOST, 2) == nullptr);
}

const FaultEvent* findEvent(
    const std::vector<FaultEvent>& events,
    EventType type,
    int slave_position)
{
    for (const FaultEvent& event : events)
    {
        if (event.type == type &&
            event.slave_position == slave_position)
        {
            return &event;
        }
    }

    return nullptr;
}

void testReportsMasterLinkDownAfterBaseline()
{
    EventDetector detector;

    const auto baseline_events =
        detector.process(makeSnapshot(true, 4, 1000));

    assert(baseline_events.empty());

    const auto events =
        detector.process(makeSnapshot(false, 4, 2000));

    assert(events.size() == 1);

    const FaultEvent& event = events[0];

    assert(event.timestamp_ms == 2000);
    assert(event.type == EventType::MASTER_LINK_DOWN);
    assert(event.slave_position == -1);
    assert(event.old_value == 1);
    assert(event.new_value == 0);
}

void testReportsMasterLinkUpAfterBaseline()
{
    EventDetector detector;

    const auto baseline_events =
        detector.process(makeSnapshot(false, 4, 1000));

    assert(baseline_events.empty());

    const auto events =
        detector.process(makeSnapshot(true, 4, 2000));

    assert(events.size() == 1);

    const FaultEvent& event = events[0];

    assert(event.timestamp_ms == 2000);
    assert(event.type == EventType::MASTER_LINK_UP);
    assert(event.slave_position == -1);
    assert(event.old_value == 0);
    assert(event.new_value == 1);
}

void testReportsSlaveCountChanged()
{
    EventDetector detector;

    const auto baseline_events =
        detector.process(
            makeSnapshot(true, 4, 1000));

    assert(baseline_events.empty());

    const auto events =
        detector.process(
            makeSnapshot(true, 2, 2000));

    assert(events.size() == 1);

    const FaultEvent& event = events[0];

    assert(event.timestamp_ms == 2000);
    assert(event.type ==
           EventType::SLAVE_COUNT_CHANGED);
    assert(event.slave_position == -1);
    assert(event.old_value == 4);
    assert(event.new_value == 2);
}

void testReportsEveryLostSlave()
{
    EventDetector detector;

    NetworkSnapshot previous =
        makeSnapshot(true, 4, 1000);

    previous.slaves = {
        makeSlave(0),
        makeSlave(1),
        makeSlave(2),
        makeSlave(3)
    };

    NetworkSnapshot current =
        makeSnapshot(true, 2, 2000);

    current.slaves = {
        makeSlave(0),
        makeSlave(1)
    };

    const auto baseline_events =
        detector.process(previous);

    assert(baseline_events.empty());

    const auto events =
        detector.process(current);

    // 一个数量变化事件，加上两个丢失事件。
    assert(events.size() == 3);

    const FaultEvent* lost_slave_2 =
        findEvent(
            events,
            EventType::SLAVE_LOST,
            2);

    const FaultEvent* lost_slave_3 =
        findEvent(
            events,
            EventType::SLAVE_LOST,
            3);

    assert(lost_slave_2 != nullptr);
    assert(lost_slave_3 != nullptr);

    assert(lost_slave_2->timestamp_ms == 2000);
    assert(lost_slave_2->old_value == 1);
    assert(lost_slave_2->new_value == 0);

    assert(lost_slave_3->timestamp_ms == 2000);
    assert(lost_slave_3->old_value == 1);
    assert(lost_slave_3->new_value == 0);
}

void testReportEveryStateChangeSlave()
{
    EventDetector detector;
    
    NetworkSnapshot previous =
        makeSnapshot(true, 4, 1000);

    previous.slaves = {
        makeSlave(0, AlState::OP),
        makeSlave(1, AlState::OP),
        makeSlave(2, AlState::OP),
        makeSlave(3, AlState::OP)
    };

    NetworkSnapshot current =
        makeSnapshot(true, 4, 2000);

    current.slaves = {
        makeSlave(0, AlState::SAFEOP),
        makeSlave(1, AlState::SAFEOP),
        makeSlave(2, AlState::OP),
        makeSlave(3, AlState::OP)
    };

    const auto baseline_events =
        detector.process(previous);

    assert(baseline_events.empty());

    const auto events =
        detector.process(current);

    // 两个状态变化事件。
    assert(events.size() == 2);
    
    const FaultEvent* state_change_slave_0 =
        findEvent(
            events,
            EventType::SLAVE_STATE_CHANGED,
            0);

    const FaultEvent* state_change_slave_1 =
        findEvent(
            events,
            EventType::SLAVE_STATE_CHANGED,
            1);

    assert(state_change_slave_0 != nullptr);
    assert(state_change_slave_1 != nullptr);

    assert(state_change_slave_0->timestamp_ms == 2000);
    assert(state_change_slave_0->old_value == (int)AlState::OP);
    assert(state_change_slave_0->new_value == (int)AlState::SAFEOP);

    assert(state_change_slave_1->timestamp_ms == 2000);
    assert(state_change_slave_1->old_value == (int)AlState::OP);
    assert(state_change_slave_1->new_value == (int)AlState::SAFEOP);
}

int main()
{
    testReportsMasterLinkDownAfterBaseline();
    testReportsMasterLinkUpAfterBaseline();
    testReportsSlaveCountChanged();
    testReportsEveryLostSlave();
    testReportEveryStateChangeSlave();
    testRescanPositionChangeDoesNotCreateFalseEvents();

    std::cout
        << "All EventDetector tests passed\n";

    return 0;
}
