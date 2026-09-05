#include "ethercat_diag/esc/ioctl_esc_register_reader.h"

#include "ethercat_diag/infrastructure/igh_ioctl_abi.h"

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <vector>

namespace
{

struct FakeRegisterDevice
{
    int open_count{};
    int open_flags{-1};
    bool fail_register_read{};
    std::uint16_t slave_position{};
    std::uint8_t emergency{255U};
    std::uint16_t address{};
    std::size_t size{};
    std::vector<std::uint8_t> response{0x13U, 0x56U};

    LinuxCalls calls()
    {
        return {
            [this](const char*, int flags) {
                ++open_count;
                open_flags = flags;
                return 17;
            },
            [this](int, unsigned long request, void* argument) {
                if (request == EC_IOCTL_MODULE)
                {
                    auto* module = static_cast<ec_ioctl_module_t*>(argument);
                    module->ioctl_version_magic =
                        igh_ioctl_abi::version_magic;
                    module->master_count = 1U;
                    return 0;
                }

                if (request == EC_IOCTL_SLAVE_REG_READ)
                {
                    if (fail_register_read)
                    {
                        errno = EIO;
                        return -1;
                    }

                    auto* reg = static_cast<ec_ioctl_slave_reg_t*>(argument);
                    slave_position = reg->slave_position;
                    emergency = reg->emergency;
                    address = reg->address;
                    size = reg->size;
                    for (std::size_t index = 0U;
                         index < reg->size;
                         ++index)
                    {
                        reg->data[index] = response.at(index);
                    }
                    return 0;
                }

                errno = ENOTTY;
                return -1;
            },
            [](int) {
                return 0;
            }
        };
    }
};

void testReadsLittleEndianUint16WithExpectedRequest()
{
    FakeRegisterDevice fake;
    IoctlEscRegisterReader reader(fake.calls());

    const RegisterReadResult result =
        reader.readU16(0, 3, 0x0110U);

    assert(result.success);
    assert(result.value == 0x5613U);
    assert(result.error.empty());
    assert(fake.open_flags == O_RDWR);
    assert(fake.slave_position == 3U);
    assert(fake.emergency == 0U);
    assert(fake.address == 0x0110U);
    assert(fake.size == 2U);
}

void testReadsRegisterBlockWithoutChangingByteOrder()
{
    FakeRegisterDevice fake;
    fake.response = {0x10U, 0x20U, 0x30U, 0x40U};
    IoctlEscRegisterReader reader(fake.calls());

    const RegisterBlockReadResult result =
        reader.readBlock(0, 2, 0x0300U, 4U);

    assert(result.success);
    assert(result.bytes == fake.response);
    assert(result.error.empty());
    assert(fake.open_flags == O_RDWR);
    assert(fake.slave_position == 2U);
    assert(fake.emergency == 0U);
    assert(fake.address == 0x0300U);
    assert(fake.size == 4U);
}

void testRejectsEmptyRegisterBlockWithoutOpening()
{
    FakeRegisterDevice fake;
    IoctlEscRegisterReader reader(fake.calls());

    const RegisterBlockReadResult result =
        reader.readBlock(0, 2, 0x0300U, 0U);

    assert(!result.success);
    assert(result.bytes.empty());
    assert(!result.error.empty());
    assert(fake.open_count == 0);
}

void testRejectsRegisterBlockPastAddressSpace()
{
    FakeRegisterDevice fake;
    IoctlEscRegisterReader reader(fake.calls());

    const RegisterBlockReadResult result =
        reader.readBlock(0, 2, 0xFFFFU, 2U);

    assert(!result.success);
    assert(result.bytes.empty());
    assert(!result.error.empty());
    assert(fake.open_count == 0);
}

void testReturnsBlockIoctlFailureWithoutPartialData()
{
    FakeRegisterDevice fake;
    fake.fail_register_read = true;
    IoctlEscRegisterReader reader(fake.calls());

    const RegisterBlockReadResult result =
        reader.readBlock(0, 3, 0x0300U, 20U);

    assert(!result.success);
    assert(result.bytes.empty());
    assert(result.error.find("EC_IOCTL_SLAVE_REG_READ") !=
           std::string::npos);
}

void testRejectsNegativeMasterWithoutOpening()
{
    FakeRegisterDevice fake;
    IoctlEscRegisterReader reader(fake.calls());

    const RegisterReadResult result =
        reader.readU16(-1, 3, 0x0110U);

    assert(!result.success);
    assert(result.value == 0U);
    assert(!result.error.empty());
    assert(fake.open_count == 0);
}

void testRejectsNegativeSlaveWithoutOpening()
{
    FakeRegisterDevice fake;
    IoctlEscRegisterReader reader(fake.calls());

    const RegisterReadResult result =
        reader.readU16(0, -1, 0x0110U);

    assert(!result.success);
    assert(result.value == 0U);
    assert(!result.error.empty());
    assert(fake.open_count == 0);
}

void testReturnsIoctlFailureWithoutThrowing()
{
    FakeRegisterDevice fake;
    fake.fail_register_read = true;
    IoctlEscRegisterReader reader(fake.calls());

    const RegisterReadResult result =
        reader.readU16(0, 3, 0x0130U);

    assert(!result.success);
    assert(result.value == 0U);
    assert(result.error.find("EC_IOCTL_SLAVE_REG_READ") !=
           std::string::npos);
}

} // namespace

int main()
{
    testReadsLittleEndianUint16WithExpectedRequest();
    testReadsRegisterBlockWithoutChangingByteOrder();
    testRejectsEmptyRegisterBlockWithoutOpening();
    testRejectsRegisterBlockPastAddressSpace();
    testReturnsBlockIoctlFailureWithoutPartialData();
    testRejectsNegativeMasterWithoutOpening();
    testRejectsNegativeSlaveWithoutOpening();
    testReturnsIoctlFailureWithoutThrowing();

    std::cout << "ioctl ESC register reader tests passed\n";
}
