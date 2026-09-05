#include "ethercat_diag/monitoring/ioctl_snapshot_reader.h"
#include "ethercat_diag/infrastructure/igh_ioctl_abi.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

namespace
{

std::string mapMasterPhase(std::uint8_t phase)
{
    switch (phase)
    {
    case 0U:
        return "Waiting for device(s)...";
    case 1U:
        return "Idle";
    case 2U:
        return "Operation";
    default:
        return "???";
    }
}

IghDeviceError invalidDataError(
    const std::string& operation,
    const std::string& message)
{
    IghDeviceError error;
    error.code = IghDeviceErrorCode::INVALID_DATA;
    error.operation = operation;
    error.message = message;
    return error;
}

AlState mapSlaveState(std::uint8_t state)
{
    switch (state)
    {
    case 0x01U:
        return AlState::INIT;
    case 0x02U:
        return AlState::PREOP;
    case 0x04U:
        return AlState::SAFEOP;
    case 0x08U:
        return AlState::OP;
    default:
        return AlState::UNKNOWN;
    }
}

std::string mapSlaveName(const ec_ioctl_slave_t& slave)
{
    const char* end = std::find(
        slave.name,
        slave.name + EC_IOCTL_STRING_SIZE,
        '\0');

    if (end != slave.name)
    {
        return std::string(slave.name, end);
    }

    std::ostringstream name;
    name
        << "0x"
        << std::hex
        << std::nouppercase
        << std::setfill('0')
        << std::setw(8)
        << slave.vendor_id
        << ":0x"
        << std::setw(8)
        << slave.product_code;
    return name.str();
}

} // namespace

IoctlSnapshotReader::IoctlSnapshotReader(LinuxCalls calls)
    : calls_(std::move(calls))
{
}

SnapshotReadResult IoctlSnapshotReader::readSnapshot(
    int master_index) const
{
    IghMasterDevice device(calls_);

    if (!device.openAndValidate(
            master_index,
            IghDeviceAccess::READ_ONLY))
    {
        return {false, {}, device.error()};
    }

    ec_ioctl_master_t raw_master{};

    if (!device.call(
            EC_IOCTL_MASTER,
            &raw_master,
            "EC_IOCTL_MASTER"))
    {
        return {false, {}, device.error()};
    }

    if (raw_master.slave_count >
        static_cast<std::uint32_t>(
            std::numeric_limits<int>::max()))
    {
        return {
            false,
            {},
            invalidDataError(
                "EC_IOCTL_MASTER",
                "slave count exceeds int range")
        };
    }

    constexpr std::uint32_t max_slave_count =
        static_cast<std::uint32_t>(
            std::numeric_limits<std::uint16_t>::max()) + 1U;

    if (raw_master.slave_count > max_slave_count)
    {
        return {
            false,
            {},
            invalidDataError(
                "EC_IOCTL_MASTER",
                "slave count exceeds position range")
        };
    }

    NetworkSnapshot candidate;
    candidate.master.phase = mapMasterPhase(raw_master.phase);
    candidate.master.active = raw_master.active != 0U;
    candidate.master.link_up =
        raw_master.devices[0].link_state != 0U;
    candidate.master.slave_count =
        static_cast<int>(raw_master.slave_count);
    candidate.master.master_index = master_index;

    int last_alias = 0;
    int relative_position = 0;

    candidate.slaves.reserve(raw_master.slave_count);

    for (std::uint32_t position = 0U;
         position < raw_master.slave_count;
         ++position)
    {
        ec_ioctl_slave_t raw_slave{};
        raw_slave.position = static_cast<std::uint16_t>(position);

        if (!device.call(
                EC_IOCTL_SLAVE,
                &raw_slave,
                "EC_IOCTL_SLAVE"))
        {
            return {false, {}, device.error()};
        }

        if (raw_slave.alias != 0U)
        {
            last_alias = static_cast<int>(raw_slave.alias);
            relative_position = 0;
        }

        candidate.slaves.push_back(
            {
                static_cast<int>(position),
                last_alias,
                relative_position,
                mapSlaveState(raw_slave.al_state),
                raw_slave.error_flag != 0U,
                true,
                mapSlaveName(raw_slave)
            });

        ++relative_position;
    }

    const auto now = std::chrono::system_clock::now();
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch());
    candidate.master.timestamp_ms =
        static_cast<std::uint64_t>(milliseconds.count());

    return {true, std::move(candidate), {}};
}
