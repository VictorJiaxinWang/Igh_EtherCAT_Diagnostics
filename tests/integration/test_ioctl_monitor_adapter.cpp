#include "ethercat_diag/monitoring/ioctl_monitor_adapter.h"

#include <cassert>
#include <iostream>
#include <utility>

namespace
{

NetworkSnapshot makeSnapshot(int master_index, int slave_count)
{
    NetworkSnapshot snapshot;
    snapshot.master.master_index = master_index;
    snapshot.master.slave_count = slave_count;
    snapshot.master.phase = "Idle";
    return snapshot;
}

void testAppliesSuccessfulSnapshotAndClearsError()
{
    NetworkSnapshot destination = makeSnapshot(88, 99);
    SnapshotReadResult result;
    result.success = true;
    result.snapshot = makeSnapshot(0, 4);

    IghDeviceError error;
    error.code = IghDeviceErrorCode::IOCTL_FAILED;

    const bool success = applySnapshotReadResult(
        std::move(result), destination, error);

    assert(success);
    assert(destination.master.master_index == 0);
    assert(destination.master.slave_count == 4);
    assert(error.code == IghDeviceErrorCode::NONE);
}

void testPreservesDestinationAndReturnsFailureError()
{
    NetworkSnapshot destination = makeSnapshot(88, 99);
    SnapshotReadResult result;
    result.success = false;
    result.error.code = IghDeviceErrorCode::IOCTL_FAILED;
    result.error.system_errno = 5;
    result.error.operation = "EC_IOCTL_MASTER";

    IghDeviceError error;

    const bool success = applySnapshotReadResult(
        std::move(result), destination, error);

    assert(!success);
    assert(destination.master.master_index == 88);
    assert(destination.master.slave_count == 99);
    assert(error.code == IghDeviceErrorCode::IOCTL_FAILED);
    assert(error.system_errno == 5);
    assert(error.operation == "EC_IOCTL_MASTER");
}

} // namespace

int main()
{
    testAppliesSuccessfulSnapshotAndClearsError();
    testPreservesDestinationAndReturnsFailureError();

    std::cout << "ioctl Monitor adapter tests passed\n";
}
