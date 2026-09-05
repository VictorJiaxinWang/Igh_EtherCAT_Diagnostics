#include "ethercat_diag/esc/port_error_decoder.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace
{

void testDecodesAllFourPortCounterOffsets()
{
    const std::vector<std::uint8_t> bytes{
        10U, 11U, 20U, 21U, 30U, 31U, 40U, 41U,
        12U, 22U, 32U, 42U,
        90U, 91U, 92U, 93U,
        13U, 23U, 33U, 43U
    };

    const PortErrorDecodeResult result =
        decodePortErrorCounters(bytes);

    assert(result.success);
    assert(result.error.empty());

    assert(result.counters.ports[0].invalid_frame == 10U);
    assert(result.counters.ports[0].rx_error == 11U);
    assert(result.counters.ports[0].forwarded_rx_error == 12U);
    assert(result.counters.ports[0].lost_link == 13U);

    assert(result.counters.ports[1].invalid_frame == 20U);
    assert(result.counters.ports[1].rx_error == 21U);
    assert(result.counters.ports[1].forwarded_rx_error == 22U);
    assert(result.counters.ports[1].lost_link == 23U);

    assert(result.counters.ports[2].invalid_frame == 30U);
    assert(result.counters.ports[2].rx_error == 31U);
    assert(result.counters.ports[2].forwarded_rx_error == 32U);
    assert(result.counters.ports[2].lost_link == 33U);

    assert(result.counters.ports[3].invalid_frame == 40U);
    assert(result.counters.ports[3].rx_error == 41U);
    assert(result.counters.ports[3].forwarded_rx_error == 42U);
    assert(result.counters.ports[3].lost_link == 43U);
}

void testRejectsARegisterBlockWithWrongSize()
{
    const PortErrorDecodeResult result =
        decodePortErrorCounters(
            std::vector<std::uint8_t>(19U, 0U));

    assert(!result.success);
    assert(!result.error.empty());
}

} // namespace

int main()
{
    testDecodesAllFourPortCounterOffsets();
    testRejectsARegisterBlockWithWrongSize();

    std::cout << "port error decoder tests passed\n";
}
