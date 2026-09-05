#include "ethercat_diag/monitoring/ethercat_reader.h"
#include "ethercat_diag/monitoring/ioctl_snapshot_reader.h"
#include "ethercat_diag/monitoring/snapshot_comparator.h"

#include <charconv>
#include <iostream>
#include <string>

namespace
{

bool parseInteger(const char* text, int& value)
{
    const std::string input(text);
    const char* begin = input.data();
    const char* end = begin + input.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr
            << "Usage: " << argv[0]
            << " <master-index> <sample-count>\n";
        return 2;
    }

    int master_index = -1;
    int sample_count = 0;

    if (!parseInteger(argv[1], master_index) || master_index < 0 ||
        !parseInteger(argv[2], sample_count) || sample_count <= 0)
    {
        std::cerr << "master-index must be non-negative and sample-count positive\n";
        return 2;
    }

    IoctlSnapshotReader ioctl_reader;
    EthercatReader shell_reader;

    const BackendComparisonStats stats = compareSnapshotBackends(
        master_index,
        sample_count,
        [&ioctl_reader](int index) {
            return ioctl_reader.readSnapshot(index);
        },
        [&shell_reader](int index, NetworkSnapshot& snapshot) {
            return shell_reader.readSnapshot(index, snapshot);
        },
        std::cout);

    return stats.mismatched == 0 ? 0 : 1;
}
