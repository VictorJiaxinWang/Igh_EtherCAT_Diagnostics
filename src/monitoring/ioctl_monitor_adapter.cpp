#include "ethercat_diag/monitoring/ioctl_monitor_adapter.h"

#include <utility>

bool applySnapshotReadResult(
    SnapshotReadResult result,
    NetworkSnapshot& destination,
    IghDeviceError& error)
{
    if (!result.success)
    {
        error = std::move(result.error);
        return false;
    }

    destination = std::move(result.snapshot);
    error = {};
    return true;
}
