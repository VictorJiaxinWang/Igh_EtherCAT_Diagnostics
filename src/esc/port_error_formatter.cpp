#include "ethercat_diag/esc/port_error_formatter.h"

#include <sstream>

std::string formatPortErrorCounters(
    const PortErrorCounters& counters)
{
    std::ostringstream output;

    for (std::size_t port = 0U;
         port < counters.ports.size();
         ++port)
    {
        if (port != 0U)
        {
            output << '\n';
        }

        const PortErrorCounter& current = counters.ports[port];
        output
            << "Port " << port
            << ": invalid_frame="
            << static_cast<unsigned int>(current.invalid_frame)
            << ", rx_error="
            << static_cast<unsigned int>(current.rx_error)
            << ", forwarded_rx_error="
            << static_cast<unsigned int>(current.forwarded_rx_error)
            << ", lost_link="
            << static_cast<unsigned int>(current.lost_link);
    }

    return output.str();
}
