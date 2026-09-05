#pragma once

#include "ethercat_diag/monitoring/ioctl_snapshot_reader.h"

bool applySnapshotReadResult(
    SnapshotReadResult result,
    NetworkSnapshot& destination,
    IghDeviceError& error);
