#pragma once

#include "ethercat_diag/infrastructure/command_runner.h"
#include "ethercat_diag/common/diag_types.h"

#include <functional>
#include <string>

class EthercatReader
{
public:
    using CommandExecutor =
        std::function<
            CommandResult(const std::string&)>;

    EthercatReader();

    explicit EthercatReader(
        CommandExecutor executor);

    bool readSnapshot(
        int master_index,
        NetworkSnapshot& snapshot);

private:
    CommandExecutor executor_;
};
