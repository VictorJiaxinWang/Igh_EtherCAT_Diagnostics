#include "ethercat_diag/esc/port_error_formatter.h"

#include <cassert>
#include <iostream>
#include <string>

int main()
{
    PortErrorCounters counters;
    counters.ports[0] = {1U, 2U, 3U, 4U};
    counters.ports[1] = {10U, 20U, 30U, 40U};
    counters.ports[2] = {0U, 0U, 0U, 0U};
    counters.ports[3] = {255U, 254U, 253U, 252U};

    const std::string formatted =
        formatPortErrorCounters(counters);

    assert(formatted ==
        "Port 0: invalid_frame=1, rx_error=2, "
        "forwarded_rx_error=3, lost_link=4\n"
        "Port 1: invalid_frame=10, rx_error=20, "
        "forwarded_rx_error=30, lost_link=40\n"
        "Port 2: invalid_frame=0, rx_error=0, "
        "forwarded_rx_error=0, lost_link=0\n"
        "Port 3: invalid_frame=255, rx_error=254, "
        "forwarded_rx_error=253, lost_link=252");

    std::cout << "port error formatter tests passed\n";
}
