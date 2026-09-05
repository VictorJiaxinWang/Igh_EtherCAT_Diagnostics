#include "ethercat_diag/esc/ioctl_esc_register_reader.h"

#include "ethercat_diag/infrastructure/igh_ioctl_abi.h"

#include <array>
#include <limits>
#include <sstream>
#include <utility>

namespace
{

RegisterReadResult failureFromDevice(
    const IghMasterDevice& device)
{
    const IghDeviceError& error = device.error();
    std::ostringstream message;

    if (!error.operation.empty())
    {
        message << error.operation;
    }

    if (!error.message.empty())
    {
        if (!error.operation.empty() &&
            error.message.find(error.operation) != 0U)
        {
            message << ": ";
        }
        message << error.message;
    }

    return {false, 0U, message.str()};
}

RegisterBlockReadResult blockFailureFromDevice(
    const IghMasterDevice& device)
{
    const RegisterReadResult failure = failureFromDevice(device);
    return {false, {}, failure.error};
}

} // namespace

IoctlEscRegisterReader::IoctlEscRegisterReader(LinuxCalls calls)
    : calls_(std::move(calls))
{
}

RegisterReadResult IoctlEscRegisterReader::readU16(
    int master_index,
    int slave_position,
    std::uint16_t address) const
{
    if (master_index < 0)
    {
        return {false, 0U, "master index must not be negative"};
    }

    if (slave_position < 0 ||
        slave_position >
            static_cast<int>(
                std::numeric_limits<std::uint16_t>::max()))
    {
        return {false, 0U, "slave position is outside uint16 range"};
    }

    IghMasterDevice device(calls_);
    if (!device.openAndValidate(
            master_index,
            IghDeviceAccess::READ_WRITE))
    {
        return failureFromDevice(device);
    }

    std::array<std::uint8_t, 2U> bytes{};
    ec_ioctl_slave_reg_t request{};
    request.slave_position =
        static_cast<std::uint16_t>(slave_position);
    request.emergency = 0U;
    request.address = address;
    request.size = bytes.size();
    request.data = bytes.data();

    if (!device.call(
            EC_IOCTL_SLAVE_REG_READ,
            &request,
            "EC_IOCTL_SLAVE_REG_READ"))
    {
        return failureFromDevice(device);
    }

    const std::uint16_t value =
        static_cast<std::uint16_t>(bytes[0]) |
        static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(bytes[1]) << 8U);

    return {true, value, {}};
}

RegisterBlockReadResult IoctlEscRegisterReader::readBlock(
    int master_index,
    int slave_position,
    std::uint16_t address,
    std::size_t size) const
{
    if (master_index < 0)
    {
        return {false, {}, "master index must not be negative"};
    }

    if (slave_position < 0 ||
        slave_position >
            static_cast<int>(
                std::numeric_limits<std::uint16_t>::max()))
    {
        return {false, {}, "slave position is outside uint16 range"};
    }

    constexpr std::size_t register_space_size = 0x10000U;
    if (size == 0U ||
        size > register_space_size -
            static_cast<std::size_t>(address))
    {
        return {false, {}, "register block is outside ESC address space"};
    }

    IghMasterDevice device(calls_);
    if (!device.openAndValidate(
            master_index,
            IghDeviceAccess::READ_WRITE))
    {
        return blockFailureFromDevice(device);
    }

    std::vector<std::uint8_t> bytes(size);
    ec_ioctl_slave_reg_t request{};
    request.slave_position =
        static_cast<std::uint16_t>(slave_position);
    request.emergency = 0U;
    request.address = address;
    request.size = bytes.size();
    request.data = bytes.data();

    if (!device.call(
            EC_IOCTL_SLAVE_REG_READ,
            &request,
            "EC_IOCTL_SLAVE_REG_READ"))
    {
        return blockFailureFromDevice(device);
    }

    return {true, std::move(bytes), {}};
}
