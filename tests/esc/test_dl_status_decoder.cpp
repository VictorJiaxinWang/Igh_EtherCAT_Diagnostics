#include "ethercat_diag/esc/dl_status_decoder.h"

#include <cassert>
#include <iostream>

using namespace std;

struct PortBitCase
{
    std::uint16_t raw;
    std::size_t expected_port;
};

void testDecodeDlState()
{
    const DlStatusInfo status = decodeDlStatus(0x5613);
    
    assert(status.raw == 0x5613);
    assert(status.pdi_operational == true);
    assert(status.pdi_watchdog_reloaded == true);
    assert(status.enhanced_link_detection == false);

    assert(status.ports[0].physical_link == true);
    assert(status.ports[0].loop_closed == false);
    assert(status.ports[0].communication_established == true);

    assert(status.ports[1].physical_link == false);
    assert(status.ports[1].loop_closed == true);
    assert(status.ports[1].communication_established == false);

    assert(status.ports[2].physical_link == false);
    assert(status.ports[2].loop_closed == true);
    assert(status.ports[2].communication_established == false);

    assert(status.ports[3].physical_link == false);
    assert(status.ports[3].loop_closed == true);
    assert(status.ports[3].communication_established == false);
}

void testDecodesEachPhysicalLinkBit()
{
    const PortBitCase cases[]{
        {0x0010, 0},
        {0x0020, 1},
        {0x0040, 2},
        {0x0080, 3}
    };

    for (const PortBitCase& test_case : cases)
    {
        const DlStatusInfo status =
            decodeDlStatus(test_case.raw);

        for (std::size_t port = 0;
             port < status.ports.size();
             ++port)
        {
            assert(
                status.ports[port].physical_link
                == (port == test_case.expected_port)
            );
        }
    }
}

void testDecodesEachLoopBit()
{
    const PortBitCase cases[]{
        {0x0100, 0},
        {0x0400, 1},
        {0x1000, 2},
        {0x4000, 3}
    };

    for (const PortBitCase& test_case : cases)
    {
        const DlStatusInfo status =
            decodeDlStatus(test_case.raw);

        for (std::size_t port = 0;
             port < status.ports.size();
             ++port)
        {
            assert(
                status.ports[port].loop_closed
                == (port == test_case.expected_port)
            );
        }
    }
}

void testDecodesEachCommunicationBit()
{
    const PortBitCase cases[]{
        {0x0200, 0},
        {0x0800, 1},
        {0x2000, 2},
        {0x8000, 3}
    };

    for (const PortBitCase& test_case : cases)
    {
        const DlStatusInfo status =
            decodeDlStatus(test_case.raw);

        for (std::size_t port = 0;
             port < status.ports.size();
             ++port)
        {
            assert(
                status.ports[port].communication_established
                == (port == test_case.expected_port)
            );
        }
    }
}


int main()
{
    testDecodeDlState();
    testDecodesEachPhysicalLinkBit();
    testDecodesEachLoopBit();
    testDecodesEachCommunicationBit();

    cout << "All the test cases passed" << endl;
    return 0;
}
