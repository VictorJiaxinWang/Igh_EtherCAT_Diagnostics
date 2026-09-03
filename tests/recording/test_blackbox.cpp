#include "ethercat_diag/recording/blackbox.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

NetworkSnapshot makeSnapshot(
    std::uint64_t timestamp_ms)
{
    NetworkSnapshot snapshot;
    snapshot.master.timestamp_ms = timestamp_ms;

    return snapshot;
}

NetworkSnapshot makeDetailedSnapshot(
    std::uint64_t timestamp_ms,
    bool link_up,
    int slave_count)
{
    NetworkSnapshot snapshot;

    snapshot.master.timestamp_ms = timestamp_ms;
    snapshot.master.master_index = 0;
    snapshot.master.link_up = link_up;
    snapshot.master.slave_count = slave_count;

    return snapshot;
}

SlaveSnapshot makeDetailedSlave(
    int position,
    int alias,
    int relative_position,
    AlState state,
    bool has_error,
    bool online,
    const std::string& name)
{
    SlaveSnapshot slave{};

    slave.position = position;
    slave.alias = alias;
    slave.relative_position = relative_position;
    slave.state = state;
    slave.has_error = has_error;
    slave.online = online;
    slave.name = name;

    return slave;
}

void testKeepsNewestSnapshotsWithinCapacity()
{
    Blackbox blackbox(3, 2);

    blackbox.push(makeSnapshot(1000));
    blackbox.push(makeSnapshot(2000));
    blackbox.push(makeSnapshot(3000));
    blackbox.push(makeSnapshot(4000));

    assert(blackbox.size() == 3);

    const auto& snapshots =
        blackbox.snapshots();

    assert(snapshots[0].master.timestamp_ms == 2000);
    assert(snapshots[1].master.timestamp_ms == 3000);
    assert(snapshots[2].master.timestamp_ms == 4000);
}

void testRejectsZeroCapacity()
{
    bool invalid_argument_thrown = false;

    try
    {
        Blackbox blackbox(0, 2);
    }
    catch (const std::invalid_argument&)
    {
        invalid_argument_thrown = true;
    }

    assert(invalid_argument_thrown);
}

void testTriggerStartsPostFaultRecording()
{
    Blackbox blackbox(3, 2);

    assert(
        blackbox.state() ==
        BlackboxState::RECORDING);

    assert(!blackbox.triggerEvent().has_value());

    const FaultEvent event{
        2500,
        EventType::SLAVE_LOST,
        2,
        1,
        0,
        "Slave 2 was lost"
    };

    blackbox.trigger(event);

    assert(
        blackbox.state() ==
        BlackboxState::POST_FAULT_RECORDING);

    assert(blackbox.triggerEvent().has_value());

    const FaultEvent& saved_event =
        *blackbox.triggerEvent();

    assert(saved_event.timestamp_ms == 2500);
    assert(saved_event.type == EventType::SLAVE_LOST);
    assert(saved_event.slave_position == 2);
    assert(saved_event.old_value == 1);
    assert(saved_event.new_value == 0);
}

void testIgnoresAdditionalTriggers()
{
    Blackbox blackbox(3, 2);

    const FaultEvent first_event{
        2500,
        EventType::SLAVE_LOST,
        2,
        1,
        0,
        "Slave 2 was lost"
    };

    const FaultEvent second_event{
        3000,
        EventType::SLAVE_STATE_CHANGED,
        1,
        static_cast<int>(AlState::OP),
        static_cast<int>(AlState::SAFEOP),
        "Slave 1 state changed"
    };

    blackbox.trigger(first_event);
    blackbox.trigger(second_event);

    assert(
        blackbox.state() ==
        BlackboxState::POST_FAULT_RECORDING);

    assert(blackbox.triggerEvent().has_value());

    const FaultEvent& saved_event =
        *blackbox.triggerEvent();

    assert(saved_event.timestamp_ms == 2500);
    assert(saved_event.type == EventType::SLAVE_LOST);
    assert(saved_event.slave_position == 2);
}

void testCollectsPostFaultSnapshotsThenFreezes()
{
    Blackbox blackbox(3, 2);

    blackbox.push(makeSnapshot(1000));
    blackbox.push(makeSnapshot(2000));
    blackbox.push(makeSnapshot(3000));

    const FaultEvent event{
        3000,
        EventType::SLAVE_LOST,
        2,
        1,
        0,
        "Slave 2 was lost"
    };

    blackbox.trigger(event);

    blackbox.push(makeSnapshot(4000));

    assert(
        blackbox.state() ==
        BlackboxState::POST_FAULT_RECORDING);

    assert(blackbox.size() == 4);

    blackbox.push(makeSnapshot(5000));

    assert(
        blackbox.state() ==
        BlackboxState::SAVING);

    assert(blackbox.size() == 5);

    // SAVING状态下缓冲区必须冻结。
    blackbox.push(makeSnapshot(6000));

    assert(blackbox.size() == 5);

    const auto& snapshots =
        blackbox.snapshots();

    assert(
        snapshots.front().master.timestamp_ms ==
        1000);

    assert(
        snapshots.back().master.timestamp_ms ==
        5000);
}

void testRejectsZeroPostFaultCapacity()
{
    bool invalid_argument_thrown = false;

    try
    {
        Blackbox blackbox(3, 0);
    }
    catch (const std::invalid_argument&)
    {
        invalid_argument_thrown = true;
    }

    assert(invalid_argument_thrown);
}

void testSavesFrozenSnapshotsAsJsonLines()
{
    Blackbox blackbox(2, 1);

    blackbox.push(
        makeDetailedSnapshot(1000, true, 4));

    blackbox.push(
        makeDetailedSnapshot(2000, true, 4));

    const FaultEvent event{
        2000,
        EventType::SLAVE_COUNT_CHANGED,
        -1,
        4,
        2,
        "Slave count changed"
    };

    blackbox.trigger(event);

    blackbox.push(
        makeDetailedSnapshot(3000, true, 2));

    assert(
        blackbox.state() ==
        BlackboxState::SAVING);

    const std::filesystem::path file_path =
        std::filesystem::temp_directory_path() /
        "ethercat_diag_blackbox_test.jsonl";

    std::filesystem::remove(file_path);

    const bool success =
        blackbox.saveToFile(file_path.string());

    assert(success);

    std::ifstream input(file_path);

    assert(input.is_open());

    std::vector<std::string> lines;
    std::string line;

    while (std::getline(input, line))
    {
        lines.push_back(line);
    }

    assert(lines.size() == 4);

    assert(
        lines[0] ==
        "{\"record\":\"snapshot\","
        "\"timestamp_ms\":1000,"
        "\"master_index\":0,"
        "\"phase\":\"\","
        "\"active\":false,"
        "\"link_up\":true,"
        "\"slave_count\":4,"
        "\"slaves\":[]}");

    assert(
        lines[1] ==
        "{\"record\":\"snapshot\","
        "\"timestamp_ms\":2000,"
        "\"master_index\":0,"
        "\"phase\":\"\","
        "\"active\":false,"
        "\"link_up\":true,"
        "\"slave_count\":4,"
        "\"slaves\":[]}");

    assert(
        lines[2] ==
        "{\"record\":\"snapshot\","
        "\"timestamp_ms\":3000,"
        "\"master_index\":0,"
        "\"phase\":\"\","
        "\"active\":false,"
        "\"link_up\":true,"
        "\"slave_count\":2,"
        "\"slaves\":[]}");

    input.close();
    std::filesystem::remove(file_path);
}

void testRefusesToSaveBeforeCaptureCompletes()
{
    Blackbox blackbox(3, 2);

    blackbox.push(
        makeDetailedSnapshot(1000, true, 4));

    assert(
        blackbox.state() ==
        BlackboxState::RECORDING);

    const std::filesystem::path file_path =
        std::filesystem::temp_directory_path() /
        "ethercat_diag_incomplete_test.jsonl";

    std::filesystem::remove(file_path);

    const bool success =
        blackbox.saveToFile(file_path.string());

    assert(!success);
    assert(!std::filesystem::exists(file_path));
}

void testSavesTriggerEventAfterSnapshots()
{
    Blackbox blackbox(2, 1);

    blackbox.push(
        makeDetailedSnapshot(1000, true, 4));

    blackbox.push(
        makeDetailedSnapshot(2000, true, 4));

    const FaultEvent event{
        2000,
        EventType::SLAVE_LOST,
        2,
        1,
        0,
        "Slave \"motor\\axis\"\n\twas lost"
    };

    blackbox.trigger(event);

    blackbox.push(
        makeDetailedSnapshot(3000, true, 2));

    assert(
        blackbox.state() ==
        BlackboxState::SAVING);

    const std::filesystem::path file_path =
        std::filesystem::temp_directory_path() /
        "ethercat_diag_event_test.jsonl";

    std::filesystem::remove(file_path);

    assert(
        blackbox.saveToFile(
            file_path.string()));

    std::ifstream input(file_path);

    assert(input.is_open());

    std::vector<std::string> lines;
    std::string line;

    while (std::getline(input, line))
    {
        lines.push_back(line);
    }

    assert(lines.size() == 4);

    assert(
        lines.back() ==
        "{\"record\":\"event\","
        "\"timestamp_ms\":2000,"
        "\"type\":\"SLAVE_LOST\","
        "\"slave_position\":2,"
        "\"old_value\":1,"
        "\"new_value\":0,"
        "\"description\":"
        "\"Slave \\\"motor\\\\axis\\\"\\n\\twas lost\"}");

    input.close();
    std::filesystem::remove(file_path);
}

void testSavesCompleteSnapshotInformation()
{
    Blackbox blackbox(1, 1);

    NetworkSnapshot fault_snapshot =
        makeDetailedSnapshot(1000, true, 2);

    fault_snapshot.master.phase =
        "Idle \"diagnostic\"";

    fault_snapshot.master.active = true;

    fault_snapshot.slaves = {
        makeDetailedSlave(
            0,
            1000,
            0,
            AlState::PREOP,
            false,
            true,
            "Drive \"A\""),

        makeDetailedSlave(
            2,
            1000,
            2,
            AlState::SAFEOP,
            true,
            false,
            "Motor\\B")
    };

    blackbox.push(fault_snapshot);

    const FaultEvent event{
        1000,
        EventType::SLAVE_STATE_CHANGED,
        2,
        static_cast<int>(AlState::OP),
        static_cast<int>(AlState::SAFEOP),
        "Slave state changed"
    };

    blackbox.trigger(event);

    blackbox.push(
        makeDetailedSnapshot(2000, true, 2));

    assert(
        blackbox.state() ==
        BlackboxState::SAVING);

    const std::filesystem::path file_path =
        std::filesystem::temp_directory_path() /
        "ethercat_diag_complete_snapshot.jsonl";

    std::filesystem::remove(file_path);

    assert(
        blackbox.saveToFile(
            file_path.string()));

    std::ifstream input(file_path);

    assert(input.is_open());

    std::string first_line;
    std::getline(input, first_line);

    assert(
        first_line ==
        "{\"record\":\"snapshot\","
        "\"timestamp_ms\":1000,"
        "\"master_index\":0,"
        "\"phase\":\"Idle \\\"diagnostic\\\"\","
        "\"active\":true,"
        "\"link_up\":true,"
        "\"slave_count\":2,"
        "\"slaves\":["
        "{\"position\":0,"
        "\"alias\":1000,"
        "\"relative_position\":0,"
        "\"state\":\"PREOP\","
        "\"has_error\":false,"
        "\"online\":true,"
        "\"name\":\"Drive \\\"A\\\"\"},"
        "{\"position\":2,"
        "\"alias\":1000,"
        "\"relative_position\":2,"
        "\"state\":\"SAFEOP\","
        "\"has_error\":true,"
        "\"online\":false,"
        "\"name\":\"Motor\\\\B\"}"
        "]}");

    input.close();
    std::filesystem::remove(file_path);
}

int main()
{
    testKeepsNewestSnapshotsWithinCapacity();
    testRejectsZeroCapacity();
    testTriggerStartsPostFaultRecording();
    testIgnoresAdditionalTriggers();
    testCollectsPostFaultSnapshotsThenFreezes();
    testRejectsZeroPostFaultCapacity();
    testSavesFrozenSnapshotsAsJsonLines();
    testRefusesToSaveBeforeCaptureCompletes();
    testSavesTriggerEventAfterSnapshots();
    testSavesCompleteSnapshotInformation();

    std::cout
        << "All Blackbox tests passed\n";

    return 0;
}
