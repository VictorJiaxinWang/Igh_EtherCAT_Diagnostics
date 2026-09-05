#include "ethercat_diag/monitoring/snapshot_comparator.h"
#include "ethercat_diag/monitoring/ioctl_snapshot_reader.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

namespace
{

NetworkSnapshot makeSnapshot()
{
    NetworkSnapshot snapshot;
    snapshot.master = {"Idle", false, true, 2, 1000U, 0};
    snapshot.slaves = {
        {0, 1000, 0, AlState::PREOP, false, true, "First"},
        {1, 1000, 1, AlState::OP, false, true, "Second"}
    };
    return snapshot;
}

bool containsField(
    const SnapshotComparison& comparison,
    const std::string& field)
{
    return std::any_of(
        comparison.differences.begin(),
        comparison.differences.end(),
        [&field](const std::string& difference) {
            return difference.find(field) != std::string::npos;
        });
}

void testIgnoresTimestampOnly()
{
    NetworkSnapshot left = makeSnapshot();
    NetworkSnapshot right = left;
    right.master.timestamp_ms = 999999U;

    const SnapshotComparison result = compareSnapshots(left, right);

    assert(result.equal);
    assert(result.differences.empty());
}

void testReportsEveryMasterFieldDifference()
{
    NetworkSnapshot left = makeSnapshot();
    NetworkSnapshot right = left;
    right.master.phase = "Operation";
    right.master.active = true;
    right.master.link_up = false;
    right.master.slave_count = 3;
    right.master.master_index = 1;

    const SnapshotComparison result = compareSnapshots(left, right);

    assert(!result.equal);
    assert(containsField(result, "master.phase"));
    assert(containsField(result, "master.active"));
    assert(containsField(result, "master.link_up"));
    assert(containsField(result, "master.slave_count"));
    assert(containsField(result, "master.master_index"));
}

void testReportsEverySlaveFieldDifference()
{
    NetworkSnapshot left = makeSnapshot();
    NetworkSnapshot right = left;
    SlaveSnapshot& slave = right.slaves[1];
    slave.position = 7;
    slave.alias = 2000;
    slave.relative_position = 0;
    slave.state = AlState::SAFEOP;
    slave.has_error = true;
    slave.online = false;
    slave.name = "Changed";

    const SnapshotComparison result = compareSnapshots(left, right);

    assert(!result.equal);
    assert(containsField(result, "slaves[1].position"));
    assert(containsField(result, "slaves[1].alias"));
    assert(containsField(result, "slaves[1].relative_position"));
    assert(containsField(result, "slaves[1].state"));
    assert(containsField(result, "slaves[1].has_error"));
    assert(containsField(result, "slaves[1].online"));
    assert(containsField(result, "slaves[1].name"));
}

void testReportsSlaveVectorSizeDifference()
{
    NetworkSnapshot left = makeSnapshot();
    NetworkSnapshot right = left;
    right.slaves.pop_back();

    const SnapshotComparison result = compareSnapshots(left, right);

    assert(!result.equal);
    assert(containsField(result, "slaves.size"));
}

void testCountsStableMatchingBackends()
{
    const NetworkSnapshot snapshot = makeSnapshot();
    int ioctl_call = 0;
    std::ostringstream output;

    const BackendComparisonStats stats = compareSnapshotBackends(
        0,
        1,
        [&](int) {
            ++ioctl_call;
            NetworkSnapshot sampled = snapshot;
            sampled.master.timestamp_ms =
                static_cast<std::uint64_t>(ioctl_call);
            return SnapshotReadResult{true, sampled, {}};
        },
        [&](int, NetworkSnapshot& sampled) {
            sampled = snapshot;
            return true;
        },
        output);

    assert(stats.matched == 1);
    assert(stats.mismatched == 0);
    assert(stats.unstable == 0);
}

void testCountsStableBackendMismatchAndPrintsField()
{
    const NetworkSnapshot ioctl_snapshot = makeSnapshot();
    NetworkSnapshot shell_snapshot = ioctl_snapshot;
    shell_snapshot.slaves[1].name = "Different";
    std::ostringstream output;

    const BackendComparisonStats stats = compareSnapshotBackends(
        0,
        1,
        [&](int) {
            return SnapshotReadResult{true, ioctl_snapshot, {}};
        },
        [&](int, NetworkSnapshot& sampled) {
            sampled = shell_snapshot;
            return true;
        },
        output);

    assert(stats.matched == 0);
    assert(stats.mismatched == 1);
    assert(stats.unstable == 0);
    assert(output.str().find("slaves[1].name") != std::string::npos);
}

void testCountsChangingIoctlSnapshotAsUnstable()
{
    NetworkSnapshot before = makeSnapshot();
    NetworkSnapshot after = before;
    after.master.link_up = false;
    int ioctl_call = 0;
    std::ostringstream output;

    const BackendComparisonStats stats = compareSnapshotBackends(
        0,
        1,
        [&](int) {
            ++ioctl_call;
            return SnapshotReadResult{
                true,
                ioctl_call == 1 ? before : after,
                {}};
        },
        [&](int, NetworkSnapshot& sampled) {
            sampled = before;
            return true;
        },
        output);

    assert(stats.matched == 0);
    assert(stats.mismatched == 0);
    assert(stats.unstable == 1);
}

void testCountsReadFailureAsUnstable()
{
    std::ostringstream output;
    const BackendComparisonStats stats = compareSnapshotBackends(
        0,
        1,
        [](int) {
            return SnapshotReadResult{};
        },
        [](int, NetworkSnapshot&) {
            return true;
        },
        output);

    assert(stats.matched == 0);
    assert(stats.mismatched == 0);
    assert(stats.unstable == 1);
}

} // namespace

int main()
{
    testIgnoresTimestampOnly();
    testReportsEveryMasterFieldDifference();
    testReportsEverySlaveFieldDifference();
    testReportsSlaveVectorSizeDifference();
    testCountsStableMatchingBackends();
    testCountsStableBackendMismatchAndPrintsField();
    testCountsChangingIoctlSnapshotAsUnstable();
    testCountsReadFailureAsUnstable();

    std::cout << "Snapshot comparator tests passed\n";
}
