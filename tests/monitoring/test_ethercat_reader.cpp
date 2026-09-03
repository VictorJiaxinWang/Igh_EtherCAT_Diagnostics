#include "ethercat_diag/monitoring/ethercat_reader.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace
{

const std::string valid_master_output =
    "Master0\n"
    "  Phase: Idle\n"
    "  Active: no\n"
    "  Slaves: 1\n"
    "  Ethernet devices:\n"
    "    Main: 00:11:22:33:44:55 (attached)\n"
    "      Link: UP\n";

const std::string valid_slaves_output =
    "0  1000:0  PREOP  +  Test Slave\n";

NetworkSnapshot makeOriginalSnapshot()
{
    NetworkSnapshot snapshot;

    snapshot.master.phase = "Original";
    snapshot.master.active = true;
    snapshot.master.link_up = false;
    snapshot.master.slave_count = 99;
    snapshot.master.timestamp_ms = 123;
    snapshot.master.master_index = 88;

    snapshot.slaves.push_back(
        {
            9,
            1000,
            9,
            AlState::OP,
            false,
            true,
            "Original Slave"
        });

    return snapshot;
}

void assertOriginalSnapshot(
    const NetworkSnapshot& snapshot)
{
    assert(snapshot.master.phase == "Original");
    assert(snapshot.master.active);
    assert(!snapshot.master.link_up);
    assert(snapshot.master.slave_count == 99);
    assert(snapshot.master.timestamp_ms == 123);
    assert(snapshot.master.master_index == 88);

    assert(snapshot.slaves.size() == 1);
    assert(snapshot.slaves[0].position == 9);
    assert(snapshot.slaves[0].name == "Original Slave");
}

}

void testReadsCompleteNetworkSnapshot()
{
    const std::string master_output =
        "Master2\n"
        "  Phase: Idle\n"
        "  Active: no\n"
        "  Slaves: 2\n"
        "  Ethernet devices:\n"
        "    Main: 00:11:22:33:44:55 (attached)\n"
        "      Link: UP\n";

    const std::string slaves_output =
        "0  1000:0  PREOP  +  First Slave\n"
        "1  1000:1  OP  +  Second Slave\n";

    std::vector<std::string> commands;

    EthercatReader reader(
        [&](const std::string& command)
            -> CommandResult
        {
            commands.push_back(command);

            if (command ==
                "ethercat master -m 2")
            {
                return {0, master_output};
            }

            if (command ==
                "ethercat slaves -m 2")
            {
                return {0, slaves_output};
            }

            return {1, "Unexpected command"};
        });

    NetworkSnapshot snapshot;

    const bool success =
        reader.readSnapshot(2, snapshot);

    assert(success);

    assert(commands.size() == 2);
    assert(commands[0] == "ethercat master -m 2");
    assert(commands[1] == "ethercat slaves -m 2");

    assert(snapshot.master.master_index == 2);
    assert(snapshot.master.timestamp_ms > 0);
    assert(snapshot.master.phase == "Idle");
    assert(!snapshot.master.active);
    assert(snapshot.master.link_up);
    assert(snapshot.master.slave_count == 2);

    assert(snapshot.slaves.size() == 2);

    assert(snapshot.slaves[0].position == 0);
    assert(snapshot.slaves[0].state == AlState::PREOP);
    assert(snapshot.slaves[0].name == "First Slave");

    assert(snapshot.slaves[1].position == 1);
    assert(snapshot.slaves[1].state == AlState::OP);
    assert(snapshot.slaves[1].name == "Second Slave");
}

void testRejectsNegativeMasterIndex()
{
    int command_count = 0;

    EthercatReader reader(
        [&](const std::string&)
            -> CommandResult
        {
            ++command_count;
            return {0, ""};
        });

    NetworkSnapshot snapshot =
        makeOriginalSnapshot();

    const bool success =
        reader.readSnapshot(-1, snapshot);

    assert(!success);
    assert(command_count == 0);
    assertOriginalSnapshot(snapshot);
}

void testStopsWhenMasterCommandFails()
{
    int command_count = 0;

    EthercatReader reader(
        [&](const std::string& command)
            -> CommandResult
        {
            ++command_count;

            assert(command ==
                "ethercat master -m 0" || command == "ethercat slaves -m 0");

            return {7, "Master command failed"};
        });

    NetworkSnapshot snapshot =
        makeOriginalSnapshot();

    const bool success =
        reader.readSnapshot(0, snapshot);

    assert(!success);
    assert(command_count == 1);
    assertOriginalSnapshot(snapshot);
}

void testStopsWhenMasterParsingFails()
{
    int command_count = 0;

    EthercatReader reader(
        [&](const std::string& command)
            -> CommandResult
        {
            ++command_count;

            assert(command ==
                "ethercat master -m 0" || command == "ethercat slaves -m 0");

            return {
                0,
                "This is not valid master output\n"
            };
        });

    NetworkSnapshot snapshot =
        makeOriginalSnapshot();

    const bool success =
        reader.readSnapshot(0, snapshot);

    assert(!success);
    assert(command_count == 1);
    assertOriginalSnapshot(snapshot);
}

void testStopsWhenSlavesCommandFails()
{
    int command_count = 0;

    EthercatReader reader(
        [&](const std::string& command)
            -> CommandResult
        {
            ++command_count;

            if (command ==
                "ethercat master -m 0")
            {
                return {
                    0,
                    valid_master_output
                };
            }

            if (command ==
                "ethercat slaves -m 0")
            {
                return {
                    8,
                    "Slaves command failed"
                };
            }

            return {
                99,
                "Unexpected command"
            };
        });

    NetworkSnapshot snapshot =
        makeOriginalSnapshot();

    const bool success =
        reader.readSnapshot(0, snapshot);

    assert(!success);
    assert(command_count == 2);
    assertOriginalSnapshot(snapshot);
}

void testRejectsInvalidSlavesOutput()
{
    int command_count = 0;

    EthercatReader reader(
        [&](const std::string& command)
            -> CommandResult
        {
            ++command_count;

            if (command ==
                "ethercat master -m 0")
            {
                return {
                    0,
                    valid_master_output
                };
            }

            if (command ==
                "ethercat slaves -m 0")
            {
                return {
                    0,
                    "Broken slave output\n"
                };
            }

            return {
                99,
                "Unexpected command"
            };
        });

    NetworkSnapshot snapshot =
        makeOriginalSnapshot();

    const bool success =
        reader.readSnapshot(0, snapshot);

    assert(!success);
    assert(command_count == 2);
    assertOriginalSnapshot(snapshot);
}

void testRejectsEmptyCommandExecutor()
{
    EthercatReader reader(
        EthercatReader::CommandExecutor{});

    NetworkSnapshot snapshot =
        makeOriginalSnapshot();

    bool threw = false;
    bool success = true;

    try
    {
        success =
            reader.readSnapshot(0, snapshot);
    }
    catch (...)
    {
        threw = true;
    }

    assert(!threw);
    assert(!success);
    assertOriginalSnapshot(snapshot);
}

int main()
{
    testReadsCompleteNetworkSnapshot();
    testRejectsNegativeMasterIndex();
    testStopsWhenMasterCommandFails();
    testStopsWhenMasterParsingFails();
    testStopsWhenSlavesCommandFails();
    testRejectsInvalidSlavesOutput();
    testRejectsEmptyCommandExecutor();

    std::cout
        << "All EthercatReader tests passed\n";

    return 0;
}
