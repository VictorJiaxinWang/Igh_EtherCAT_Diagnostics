#include "ethercat_diag/monitoring/snapshot_comparator.h"

#include <algorithm>
#include <ostream>
#include <string>

namespace
{

void addDifference(
    bool values_differ,
    const std::string& field,
    SnapshotComparison& comparison)
{
    if (values_differ)
    {
        comparison.differences.push_back(field);
    }
}

} // namespace

SnapshotComparison compareSnapshots(
    const NetworkSnapshot& left,
    const NetworkSnapshot& right)
{
    SnapshotComparison comparison;

    addDifference(
        left.master.phase != right.master.phase,
        "master.phase",
        comparison);
    addDifference(
        left.master.active != right.master.active,
        "master.active",
        comparison);
    addDifference(
        left.master.link_up != right.master.link_up,
        "master.link_up",
        comparison);
    addDifference(
        left.master.slave_count != right.master.slave_count,
        "master.slave_count",
        comparison);
    addDifference(
        left.master.master_index != right.master.master_index,
        "master.master_index",
        comparison);

    addDifference(
        left.slaves.size() != right.slaves.size(),
        "slaves.size",
        comparison);

    const std::size_t common_size =
        std::min(left.slaves.size(), right.slaves.size());

    for (std::size_t index = 0; index < common_size; ++index)
    {
        const SlaveSnapshot& left_slave = left.slaves[index];
        const SlaveSnapshot& right_slave = right.slaves[index];
        const std::string prefix =
            "slaves[" + std::to_string(index) + "].";

        addDifference(
            left_slave.position != right_slave.position,
            prefix + "position",
            comparison);
        addDifference(
            left_slave.alias != right_slave.alias,
            prefix + "alias",
            comparison);
        addDifference(
            left_slave.relative_position != right_slave.relative_position,
            prefix + "relative_position",
            comparison);
        addDifference(
            left_slave.state != right_slave.state,
            prefix + "state",
            comparison);
        addDifference(
            left_slave.has_error != right_slave.has_error,
            prefix + "has_error",
            comparison);
        addDifference(
            left_slave.online != right_slave.online,
            prefix + "online",
            comparison);
        addDifference(
            left_slave.name != right_slave.name,
            prefix + "name",
            comparison);
    }

    comparison.equal = comparison.differences.empty();
    return comparison;
}

BackendComparisonStats compareSnapshotBackends(
    int master_index,
    int sample_count,
    const IoctlSnapshotRead& ioctl_reader,
    const ShellSnapshotRead& shell_reader,
    std::ostream& output)
{
    BackendComparisonStats stats;

    for (int sample = 0; sample < sample_count; ++sample)
    {
        const SnapshotReadResult before =
            ioctl_reader(master_index);

        NetworkSnapshot shell_snapshot;
        const bool shell_success =
            shell_reader(master_index, shell_snapshot);

        const SnapshotReadResult after =
            ioctl_reader(master_index);

        if (!before.success || !shell_success || !after.success)
        {
            ++stats.unstable;
            output << "sample " << sample << ": read failed\n";
            continue;
        }

        const SnapshotComparison stability =
            compareSnapshots(before.snapshot, after.snapshot);

        if (!stability.equal)
        {
            ++stats.unstable;
            output << "sample " << sample << ": unstable";
            for (const std::string& field : stability.differences)
            {
                output << ' ' << field;
            }
            output << '\n';
            continue;
        }

        const SnapshotComparison backends =
            compareSnapshots(shell_snapshot, after.snapshot);

        if (backends.equal)
        {
            ++stats.matched;
            continue;
        }

        ++stats.mismatched;
        output << "sample " << sample << ": mismatch";
        for (const std::string& field : backends.differences)
        {
            output << ' ' << field;
        }
        output << '\n';
    }

    output
        << "matched=" << stats.matched
        << " mismatched=" << stats.mismatched
        << " unstable=" << stats.unstable
        << '\n';

    return stats;
}
