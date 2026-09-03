#pragma once

#include "ethercat_diag/infrastructure/command_runner.h"

#include <cstdint>
#include <functional>
#include <string>

struct RegisterReadResult
{
    bool success{false};
    std::uint16_t value{};
    std::string error;
};

class EscRegisterReader
{
public:
    using CommandExecutor =
        std::function<
            CommandResult(const std::string&)>;

    EscRegisterReader();

    explicit EscRegisterReader(
        CommandExecutor executor);

    RegisterReadResult readU16(
        int master_index,
        int slave_position,
        std::uint16_t address) const;

private:
    CommandExecutor executor_;
};
