#include "ethercat_diag/monitoring/slave_parser.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

void testParseValidSlaveOutput()
{
    const std::string input = 
        "  0  1000:0  PREOP  +  IF1100_6SW(X4,X5,X6)_1.4.2.3\n"
        "  1  1000:1  PREOP  +  Renesas EtherCAT RZ/T2 1-5ARM Foe V1.0.1\n";

    std::vector<SlaveSnapshot> snapshots;

    const bool success = parseSlavesOutput(input, snapshots);

    assert(success);
    assert(snapshots.size() == 2);

    for(std::size_t i = 0; i < snapshots.size(); i++)
    {
        assert(snapshots[i].position == (int)i);
        assert(snapshots[i].alias == 1000);
        assert(snapshots[i].relative_position == (int)i);
        assert(snapshots[i].state == AlState::PREOP);
        assert(snapshots[i].has_error == false);
        assert(snapshots[i].online == true);
    }
    assert(snapshots[0].name == "IF1100_6SW(X4,X5,X6)_1.4.2.3");
    assert(snapshots[1].name == "Renesas EtherCAT RZ/T2 1-5ARM Foe V1.0.1");
}

void testRejectsInvalidNumbersWithoutThrowing()
{
    const std::vector<std::string> invalid_inputs{
        "abc  1000:0  PREOP  +  Slave\n",
        "0  abc:0  PREOP  +  Slave\n",
        "0  1000:abc  PREOP  +  Slave\n",
        "0  999999999999999999999:0  PREOP  +  Slave\n",
        "0  1000:0abc  PREOP  +  Slave\n"
    };

    for (const std::string& input : invalid_inputs)
    {
        std::vector<SlaveSnapshot> snapshots;

        bool threw = false;
        bool success = true;

        try
        {
            success =
                parseSlavesOutput(input, snapshots);
        }
        catch (...)
        {
            threw = true;
        }

        assert(!threw);
        assert(!success);
    }
}


void testFailureDoesNotModifyOutput()
{
    const std::string input =
        "0  1000:0  PREOP  X  Invalid Slave\n";

    std::vector<SlaveSnapshot> snapshots{
        {
            99,
            88,
            77,
            AlState::OP,
            false,
            true,
            "Original"
        }
    };

    const bool success =
        parseSlavesOutput(input, snapshots);

    assert(!success);

    assert(snapshots.size() == 1);
    assert(snapshots[0].position == 99);
    assert(snapshots[0].alias == 88);
    assert(snapshots[0].relative_position == 77);
    assert(snapshots[0].state == AlState::OP);
    assert(snapshots[0].name == "Original");
}

void testRepeatedCallsReplaceResults()
{
    const std::string first_input =
        "0  1000:0  PREOP  +  First\n"
        "1  1000:1  PREOP  +  Second\n";

    const std::string second_input =
        "3  1000:3  OP  +  Third\n";

    std::vector<SlaveSnapshot> snapshots;

    assert(parseSlavesOutput(
        first_input,
        snapshots));

    assert(snapshots.size() == 2);

    assert(parseSlavesOutput(
        second_input,
        snapshots));

    assert(snapshots.size() == 1);
    assert(snapshots[0].position == 3);
    assert(snapshots[0].name == "Third");
}

void testEmptyOutputMeansZeroSlaves()
{
    std::vector<SlaveSnapshot> snapshots{
        {
            99,
            88,
            77,
            AlState::OP,
            false,
            true,
            "Old Slave"
        }
    };

    const bool success =
        parseSlavesOutput("", snapshots);

    assert(success);
    assert(snapshots.empty());
}

void testUnknownStateIsPreserved()
{
    const std::string input =
        "0  1000:0  NEWSTATE  +  Slave\n";

    std::vector<SlaveSnapshot> snapshots;

    const bool success =
        parseSlavesOutput(input, snapshots);

    assert(success);
    assert(snapshots.size() == 1);
    assert(snapshots[0].state == AlState::UNKNOWN);
}

void testRejectsMissingName()
{
    const std::string input =
        "0  1000:0  PREOP  +\n";

    std::vector<SlaveSnapshot> snapshots;

    const bool success =
        parseSlavesOutput(input, snapshots);

    assert(!success);
}

int main()
{
    testParseValidSlaveOutput();
    testRejectsInvalidNumbersWithoutThrowing();
    testFailureDoesNotModifyOutput();
    testRepeatedCallsReplaceResults();
    testEmptyOutputMeansZeroSlaves();
    testUnknownStateIsPreserved();
    testRejectsMissingName();

    std::cout << "All tests passed!\n";
    return 0;
}