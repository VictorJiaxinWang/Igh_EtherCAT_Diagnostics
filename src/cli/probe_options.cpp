#include "ethercat_diag/cli/probe_options.h"

#include <limits>
#include <stdexcept>

namespace
{

std::optional<int> parseNonNegativeInteger(
    const std::string& text
)
{
    std::size_t consumed = 0;
    long long value = 0;

    try
    {
        value = std::stoll(text, &consumed, 10);
    }
    catch (const std::invalid_argument&)
    {
        return std::nullopt;
    }
    catch (const std::out_of_range&)
    {
        return std::nullopt;
    }

    if (consumed != text.size())
    {
        return std::nullopt;
    }

    if (value < 0)
    {
        return std::nullopt;
    }

    if (value > std::numeric_limits<int>::max())
    {
        return std::nullopt;
    }

    return static_cast<int>(value);
}

} // namespace

std::optional<ProbeOptions> parseProbeOptions(
    const std::vector<std::string>& arguments
)
{
    if (arguments.size() != 2)
    {
        return std::nullopt;
    }

    const std::optional<int> master_index =
        parseNonNegativeInteger(arguments[0]);

    const std::optional<int> slave_position =
        parseNonNegativeInteger(arguments[1]);

    if (!master_index || !slave_position)
    {
        return std::nullopt;
    }

    return ProbeOptions{
        *master_index,
        *slave_position
    };
}
