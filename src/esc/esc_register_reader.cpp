#include "ethercat_diag/esc/esc_register_reader.h"

#include <limits>
#include <optional>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

std::optional<unsigned long> parseUnsignedLongToken(
    const std::string& text,
    int base
)
{
    std::size_t consumed = 0;

    try
    {
        const unsigned long value =
            std::stoul(text, &consumed, base);

        if (consumed != text.size())
        {
            return std::nullopt;
        }

        return value;
    }
    catch (const std::invalid_argument&)
    {
        return std::nullopt;
    }
    catch (const std::out_of_range&)
    {
        return std::nullopt;
    }
}

} // namespace

EscRegisterReader::EscRegisterReader()
    : executor_(execCommand)
{
}

EscRegisterReader::EscRegisterReader(
    CommandExecutor executor)
    : executor_(std::move(executor))
{
}

RegisterReadResult
EscRegisterReader::readU16(
    int master_index,
    int slave_position,
    std::uint16_t address) const
{
    std::ostringstream command;

    if (master_index < 0)
    {
        return {
            false,
            0,
            "master index must not be negative"
        };
    }

    if (slave_position < 0)
    {
        return {
            false,
            0,
            "slave position must not be negative"
        };
    }

    if (!executor_)
    {
        return {
            false,
            0,
            "command executor is empty"
        };
    }

    command
        << "ethercat reg_read "
        << "-m " << master_index
        << " -p " << slave_position
        << " -t uint16 "
        << "0x"
        << std::hex
        << std::nouppercase
        << std::setfill('0')
        << std::setw(4)
        << address;

    const CommandResult command_result =
        executor_(command.str());

    if (command_result.exit_code != 0)
    {
        return RegisterReadResult{
            false,
            0,
            "ethercat reg_read failed with exit code " +
                std::to_string(
                    command_result.exit_code)
        };
    }

    std::istringstream output_stream(
        command_result.output
    );

    std::vector<std::string> tokens;
    std::string token;

    while (output_stream >> token)
    {
        tokens.push_back(token);
    }

    if (tokens.empty() || tokens.size() > 2)
    {
        return {
            false,
            0,
            "register output must contain one or two values"
        };
    }

    const std::optional<unsigned long> primary_value =
        parseUnsignedLongToken(tokens[0], 0);

    if (!primary_value)
    {
        return {
            false,
            0,
            "register output contains an invalid primary value"
        };
    }

    if (tokens.size() == 2)
    {
        const std::optional<unsigned long> secondary_value =
            parseUnsignedLongToken(tokens[1], 10);

        if (!secondary_value)
        {
            return {
                false,
                0,
                "register output contains an invalid secondary value"
            };
        }

        if (*secondary_value != *primary_value)
        {
            return {
                false,
                0,
                "register output values are inconsistent"
            };
        }
    }

    if (*primary_value >
        std::numeric_limits<std::uint16_t>::max())
    {
        return {
            false,
            0,
            "register value exceeds uint16_t range"
        };
    }

    return RegisterReadResult{
        true,
        static_cast<std::uint16_t>(
            *primary_value),
        {}
    };
}
