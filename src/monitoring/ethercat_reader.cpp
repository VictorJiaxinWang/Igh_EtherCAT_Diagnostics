#include "ethercat_diag/monitoring/ethercat_reader.h"

#include "ethercat_diag/monitoring/master_parser.h"
#include "ethercat_diag/monitoring/slave_parser.h"

#include <chrono>
#include <string>
#include <utility>

EthercatReader::EthercatReader()
    : executor_(execCommand)
{
}

EthercatReader::EthercatReader(
    CommandExecutor executor)
    : executor_(std::move(executor))
{
}

bool EthercatReader::readSnapshot(
    int master_index,
    NetworkSnapshot& snapshot)
{
    if (!executor_)
    {
        return false;
    }

    if (master_index < 0)  // avoid negative master index
    {
        return false;
    }

    NetworkSnapshot candidate;
    
    const std::string master_command =
        "ethercat master -m "
        + std::to_string(master_index);

    const CommandResult master_result =
        executor_(master_command);

    if (master_result.exit_code != 0)
    {
        return false;
    }

    if(!parseMasterOutput(
        master_result.output,
        candidate.master))
    {
        return false;
    }

    const std::string slaves_command =
        "ethercat slaves -m "
        + std::to_string(master_index);

    const CommandResult slaves_result =
        executor_(slaves_command);

    if (slaves_result.exit_code != 0)
    {
        return false;
    }

    if (!parseSlavesOutput(
        slaves_result.output,
        candidate.slaves))
    {
        return false;
    }

    const auto now =
        std::chrono::system_clock::now();

    const auto milliseconds =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
                now.time_since_epoch());

    candidate.master.timestamp_ms =
        static_cast<std::uint64_t>(
            milliseconds.count());

    candidate.master.master_index =
        master_index;

    snapshot = std::move(candidate);

    return true;
}
