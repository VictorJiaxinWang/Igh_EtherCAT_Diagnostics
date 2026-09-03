#include "ethercat_diag/recording/blackbox_filename.h"

#include <cstddef>
#include <string>

std::filesystem::path makeUniqueBlackboxPath(
    const std::filesystem::path& directory,
    std::uint64_t timestamp_ms)
{
    const std::string filename_stem =
        "fault_" +
        std::to_string(timestamp_ms);

    std::filesystem::path candidate =
        directory /
        (filename_stem + ".jsonl");

    std::size_t suffix = 1;

    while (std::filesystem::exists(candidate))
    {
        candidate =
            directory /
            (filename_stem +
             "_" +
             std::to_string(suffix) +
             ".jsonl");

        ++suffix;
    }

    return candidate;
}