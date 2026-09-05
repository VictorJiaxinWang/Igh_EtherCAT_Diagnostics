#include "ethercat_diag/esc/port_error_reader.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{

std::vector<std::uint8_t> makeRegisterBlock()
{
    return {
        1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U,
        9U, 10U, 11U, 12U,
        0U, 0U, 0U, 0U,
        13U, 14U, 15U, 16U
    };
}

void testReadsAndDecodesTheStandardRegisterBlock()
{
    int captured_master = -1;
    int captured_slave = -1;
    std::uint16_t captured_address = 0U;
    std::size_t captured_size = 0U;

    PortErrorReader reader(
        [&](int master, int slave, std::uint16_t address,
            std::size_t size) {
            captured_master = master;
            captured_slave = slave;
            captured_address = address;
            captured_size = size;
            return RegisterBlockReadResult{
                true, makeRegisterBlock(), {}};
        });

    const PortErrorReadResult result = reader.read(2, 5);

    assert(result.success);
    assert(result.error.empty());
    assert(result.counters.ports[0].invalid_frame == 1U);
    assert(result.counters.ports[3].lost_link == 16U);
    assert(captured_master == 2);
    assert(captured_slave == 5);
    assert(captured_address == 0x0300U);
    assert(captured_size == 20U);
}

void testPreservesBlockReadFailure()
{
    PortErrorReader reader(
        [](int, int, std::uint16_t, std::size_t) {
            return RegisterBlockReadResult{
                false, {}, "device read failed"};
        });

    const PortErrorReadResult result = reader.read(0, 3);

    assert(!result.success);
    assert(result.error == "device read failed");
}

} // namespace

int main()
{
    testReadsAndDecodesTheStandardRegisterBlock();
    testPreservesBlockReadFailure();
    std::cout << "port error reader tests passed\n";
}
