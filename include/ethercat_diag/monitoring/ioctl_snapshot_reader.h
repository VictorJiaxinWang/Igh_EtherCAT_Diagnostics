#pragma once

#include "ethercat_diag/common/diag_types.h"
#include "ethercat_diag/infrastructure/igh_master_device.h"

struct SnapshotReadResult
{
    bool success{false};
    NetworkSnapshot snapshot;
    IghDeviceError error;
};

class IoctlSnapshotReader
{
public:
    explicit IoctlSnapshotReader(
        LinuxCalls calls = productionLinuxCalls());

    SnapshotReadResult readSnapshot(
        int master_index) const;

private:
    LinuxCalls calls_;
};
