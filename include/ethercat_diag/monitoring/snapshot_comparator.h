#pragma once

#include "ethercat_diag/common/diag_types.h"
#include "ethercat_diag/monitoring/ioctl_snapshot_reader.h"

#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

struct SnapshotComparison
{
    bool equal{false};
    std::vector<std::string> differences;
};

SnapshotComparison compareSnapshots(
    const NetworkSnapshot& left,
    const NetworkSnapshot& right);

struct BackendComparisonStats
{
    int matched{};
    int mismatched{};
    int unstable{};
};

using IoctlSnapshotRead =
    std::function<SnapshotReadResult(int)>;

using ShellSnapshotRead =
    std::function<bool(int, NetworkSnapshot&)>;

BackendComparisonStats compareSnapshotBackends(
    int master_index,
    int sample_count,
    const IoctlSnapshotRead& ioctl_reader,
    const ShellSnapshotRead& shell_reader,
    std::ostream& output);
