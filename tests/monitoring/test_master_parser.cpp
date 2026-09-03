#include "ethercat_diag/monitoring/master_parser.h"

#include <cassert>
#include <iostream>
#include <string>

void testDoesNotModifySnapshotWhenFieldIsMissing()
{
    const std::string input =
        "Master0\n"
        "  Phase: Idle\n"
        "  Active: no\n"
        "  Slaves: 4\n";

    MasterSnapshot snapshot{
        "Original",
        true,
        true,
        99
    };

    const bool success =
        parseMasterOutput(input, snapshot);

    assert(!success);
    assert(snapshot.phase == "Original");
    assert(snapshot.active);
    assert(snapshot.link_up);
    assert(snapshot.slave_count == 99);
}

void testParsesValidMasterOutput()
{
    const std::string input =
        "Master0\n"
        "  Phase: Idle\n"
        "  Active: no\n"
        "  Slaves: 4\n"
        "  Ethernet devices:\n"
        "    Main: c2:a8:ba:1b:9f:7f (attached)\n"
        "      Link: UP\n";

    MasterSnapshot snapshot;

    const bool success =
        parseMasterOutput(input, snapshot);

    assert(success);
    assert(snapshot.phase == "Idle");
    assert(!snapshot.active);
    assert(snapshot.link_up);
    assert(snapshot.slave_count == 4);
}

int main()
{
    testParsesValidMasterOutput();
    testDoesNotModifySnapshotWhenFieldIsMissing();

    std::cout << "All tests passed\n";
    return 0;
}
