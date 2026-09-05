#include "ethercat_diag/monitoring/ioctl_snapshot_reader.h"

#include "ethercat_diag/infrastructure/igh_ioctl_abi.h"

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{

LinuxCalls makeLinuxCalls(
    const ec_ioctl_master_t& master,
    const std::vector<ec_ioctl_slave_t>& slaves = {},
    bool fail_master = false,
    int fail_slave_position = -1)
{
    return {
        [](const char*, int) {
            return 9;
        },
        [master, slaves, fail_master, fail_slave_position](
            int,
            unsigned long request,
            void* argument) {
            if (request == EC_IOCTL_MODULE)
            {
                auto* module = static_cast<ec_ioctl_module_t*>(argument);
                module->ioctl_version_magic =
                    igh_ioctl_abi::version_magic;
                module->master_count = 1;
                return 0;
            }

            if (request == EC_IOCTL_SLAVE)
            {
                auto* slave = static_cast<ec_ioctl_slave_t*>(argument);
                const int position = slave->position;

                if (position == fail_slave_position)
                {
                    errno = EIO;
                    return -1;
                }

                if (position < 0 ||
                    static_cast<std::size_t>(position) >= slaves.size())
                {
                    errno = EINVAL;
                    return -1;
                }

                *slave = slaves[static_cast<std::size_t>(position)];
                return 0;
            }

            if (request == EC_IOCTL_MASTER)
            {
                if (fail_master)
                {
                    errno = EIO;
                    return -1;
                }

                *static_cast<ec_ioctl_master_t*>(argument) = master;
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

void testMapsAllMasterPhases()
{
    struct Case
    {
        std::uint8_t raw;
        const char* expected;
    };

    const Case cases[] = {
        {0U, "Waiting for device(s)..."},
        {1U, "Idle"},
        {2U, "Operation"},
        {99U, "???"}
    };

    for (const Case& test_case : cases)
    {
        ec_ioctl_master_t raw{};
        raw.phase = test_case.raw;

        IoctlSnapshotReader reader(makeLinuxCalls(raw));
        const SnapshotReadResult result = reader.readSnapshot(4);

        assert(result.success);
        assert(result.snapshot.master.phase == test_case.expected);
        assert(result.snapshot.master.master_index == 4);
        assert(result.snapshot.master.timestamp_ms > 0U);
    }
}

void testMapsMasterFlagsAndSlaveCount()
{
    ec_ioctl_master_t raw{};
    raw.phase = 1U;
    raw.active = 7U;
    raw.devices[0].link_state = 1U;
    raw.slave_count = 2U;

    IoctlSnapshotReader reader(
        makeLinuxCalls(raw, std::vector<ec_ioctl_slave_t>(2)));
    const SnapshotReadResult result = reader.readSnapshot(0);

    assert(result.success);
    assert(result.snapshot.master.active);
    assert(result.snapshot.master.link_up);
    assert(result.snapshot.master.slave_count == 2);
}

void testMapsFalseMasterFlags()
{
    ec_ioctl_master_t raw{};
    raw.phase = 1U;

    IoctlSnapshotReader reader(makeLinuxCalls(raw));
    const SnapshotReadResult result = reader.readSnapshot(0);

    assert(result.success);
    assert(!result.snapshot.master.active);
    assert(!result.snapshot.master.link_up);
}

void testRejectsNegativeMasterIndex()
{
    ec_ioctl_master_t raw{};
    IoctlSnapshotReader reader(makeLinuxCalls(raw));

    const SnapshotReadResult result = reader.readSnapshot(-1);

    assert(!result.success);
    assert(result.error.code == IghDeviceErrorCode::INVALID_MASTER_INDEX);
}

void testRejectsSlaveCountOutsideIntRange()
{
    ec_ioctl_master_t raw{};
    raw.slave_count = std::numeric_limits<std::uint32_t>::max();
    IoctlSnapshotReader reader(makeLinuxCalls(raw));

    const SnapshotReadResult result = reader.readSnapshot(0);

    assert(!result.success);
    assert(result.error.code == IghDeviceErrorCode::INVALID_DATA);
    assert(result.snapshot.slaves.empty());
}

void testRejectsSlaveCountOutsidePositionRange()
{
    ec_ioctl_master_t raw{};
    raw.slave_count =
        static_cast<std::uint32_t>(
            std::numeric_limits<std::uint16_t>::max()) + 2U;
    IoctlSnapshotReader reader(makeLinuxCalls(raw));

    const SnapshotReadResult result = reader.readSnapshot(0);

    assert(!result.success);
    assert(result.error.code == IghDeviceErrorCode::INVALID_DATA);
    assert(result.error.operation == "EC_IOCTL_MASTER");
}

void testReportsMasterIoctlFailure()
{
    ec_ioctl_master_t raw{};
    IoctlSnapshotReader reader(makeLinuxCalls(raw, {}, true));

    const SnapshotReadResult result = reader.readSnapshot(0);

    assert(!result.success);
    assert(result.error.code == IghDeviceErrorCode::IOCTL_FAILED);
    assert(result.error.system_errno == EIO);
    assert(result.error.operation == "EC_IOCTL_MASTER");
}

void testMapsAliasInheritanceAndRelativePositions()
{
    ec_ioctl_master_t master{};
    master.phase = 1U;
    master.slave_count = 4U;

    std::vector<ec_ioctl_slave_t> slaves(4);
    slaves[0].alias = 1000U;
    slaves[1].alias = 0U;
    slaves[2].alias = 2000U;
    slaves[3].alias = 0U;

    IoctlSnapshotReader reader(makeLinuxCalls(master, slaves));
    const SnapshotReadResult result = reader.readSnapshot(0);

    assert(result.success);
    assert(result.snapshot.slaves.size() == 4U);
    assert(result.snapshot.slaves[0].alias == 1000);
    assert(result.snapshot.slaves[0].relative_position == 0);
    assert(result.snapshot.slaves[1].alias == 1000);
    assert(result.snapshot.slaves[1].relative_position == 1);
    assert(result.snapshot.slaves[2].alias == 2000);
    assert(result.snapshot.slaves[2].relative_position == 0);
    assert(result.snapshot.slaves[3].alias == 2000);
    assert(result.snapshot.slaves[3].relative_position == 1);
}

void testMapsAllSlaveStates()
{
    ec_ioctl_master_t master{};
    master.slave_count = 6U;

    std::vector<ec_ioctl_slave_t> slaves(6);
    const std::uint8_t raw_states[] = {
        0x01U, 0x02U, 0x04U, 0x08U, 0x03U, 0xffU
    };
    const AlState expected_states[] = {
        AlState::INIT,
        AlState::PREOP,
        AlState::SAFEOP,
        AlState::OP,
        AlState::UNKNOWN,
        AlState::UNKNOWN
    };

    for (std::size_t index = 0; index < slaves.size(); ++index)
    {
        slaves[index].al_state = raw_states[index];
    }

    IoctlSnapshotReader reader(makeLinuxCalls(master, slaves));
    const SnapshotReadResult result = reader.readSnapshot(0);

    assert(result.success);
    for (std::size_t index = 0; index < slaves.size(); ++index)
    {
        assert(result.snapshot.slaves[index].state == expected_states[index]);
    }
}

void testMapsNameErrorAndOnlineFields()
{
    ec_ioctl_master_t master{};
    master.slave_count = 2U;

    std::vector<ec_ioctl_slave_t> slaves(2);
    std::strncpy(slaves[0].name, "Named Slave", sizeof(slaves[0].name) - 1U);
    slaves[0].error_flag = 1U;
    slaves[1].vendor_id = 0xabU;
    slaves[1].product_code = 0x1234U;

    IoctlSnapshotReader reader(makeLinuxCalls(master, slaves));
    const SnapshotReadResult result = reader.readSnapshot(0);

    assert(result.success);
    assert(result.snapshot.slaves[0].position == 0);
    assert(result.snapshot.slaves[0].name == "Named Slave");
    assert(result.snapshot.slaves[0].has_error);
    assert(result.snapshot.slaves[0].online);
    assert(result.snapshot.slaves[1].position == 1);
    assert(result.snapshot.slaves[1].name == "0x000000ab:0x00001234");
    assert(!result.snapshot.slaves[1].has_error);
    assert(result.snapshot.slaves[1].online);
}

void testDiscardsCandidateWhenSlaveReadFails()
{
    ec_ioctl_master_t master{};
    master.slave_count = 3U;
    std::vector<ec_ioctl_slave_t> slaves(3);

    IoctlSnapshotReader reader(
        makeLinuxCalls(master, slaves, false, 1));
    const SnapshotReadResult result = reader.readSnapshot(0);

    assert(!result.success);
    assert(result.error.code == IghDeviceErrorCode::IOCTL_FAILED);
    assert(result.error.operation == "EC_IOCTL_SLAVE");
    assert(result.snapshot.master.slave_count == 0);
    assert(result.snapshot.slaves.empty());
}

} // namespace

int main()
{
    testMapsAllMasterPhases();
    testMapsMasterFlagsAndSlaveCount();
    testMapsFalseMasterFlags();
    testRejectsNegativeMasterIndex();
    testRejectsSlaveCountOutsideIntRange();
    testRejectsSlaveCountOutsidePositionRange();
    testReportsMasterIoctlFailure();
    testMapsAliasInheritanceAndRelativePositions();
    testMapsAllSlaveStates();
    testMapsNameErrorAndOnlineFields();
    testDiscardsCandidateWhenSlaveReadFails();

    std::cout << "IoctlSnapshotReader Master tests passed\n";
}
