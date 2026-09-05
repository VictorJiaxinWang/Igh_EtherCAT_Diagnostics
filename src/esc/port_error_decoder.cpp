#include "ethercat_diag/esc/port_error_decoder.h"

PortErrorDecodeResult decodePortErrorCounters(
    const std::vector<std::uint8_t>& bytes)
{
    if (bytes.size() != esc_port_error_block_size)
    {
        return {
            false,
            {},
            "port error register block must contain 20 bytes"
        };
    }

    PortErrorCounters counters;

    for (std::size_t port = 0U;
         port < counters.ports.size();
         ++port)
    {
        PortErrorCounter& current = counters.ports[port];
        current.invalid_frame = bytes[port * 2U];
        current.rx_error = bytes[port * 2U + 1U];
        current.forwarded_rx_error = bytes[8U + port];
        current.lost_link = bytes[16U + port];
    }

    return {true, counters, {}};
}
