#include "ethercat_diag/web/web_options.h"

#include <charconv>
#include <limits>

namespace
{

bool takeValue(
    const std::vector<std::string>& arguments,
    std::size_t& index,
    std::string& value,
    std::string& error)
{
    if (index + 1U >= arguments.size())
    {
        error = "missing value for " + arguments[index];
        return false;
    }

    value = arguments[++index];
    return true;
}

bool parsePort(
    const std::string& text,
    std::uint16_t& port)
{
    unsigned int value{};
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto parsed = std::from_chars(begin, end, value);

    if (parsed.ec != std::errc{} ||
        parsed.ptr != end ||
        value == 0U ||
        value > std::numeric_limits<std::uint16_t>::max())
    {
        return false;
    }

    port = static_cast<std::uint16_t>(value);
    return true;
}

}

WebOptionsParseResult parseWebOptions(
    const std::vector<std::string>& arguments)
{
    WebOptionsParseResult result;
    result.success = true;

    for (std::size_t index = 0U; index < arguments.size(); ++index)
    {
        const std::string& argument = arguments[index];
        std::string value;

        if (argument == "--help")
        {
            result.show_help = true;
            continue;
        }

        if (argument == "--bind")
        {
            if (!takeValue(arguments, index, value, result.error))
            {
                result.success = false;
                return result;
            }
            result.options.bind_address = value;
            continue;
        }

        if (argument == "--port")
        {
            if (!takeValue(arguments, index, value, result.error))
            {
                result.success = false;
                return result;
            }
            if (!parsePort(value, result.options.port))
            {
                result.success = false;
                result.error = "invalid port: " + value;
                return result;
            }
            continue;
        }

        if (argument == "--data-dir")
        {
            if (!takeValue(arguments, index, value, result.error))
            {
                result.success = false;
                return result;
            }
            result.options.data_directory = value;
            continue;
        }

        if (argument == "--assets-dir")
        {
            if (!takeValue(arguments, index, value, result.error))
            {
                result.success = false;
                return result;
            }
            result.options.assets_directory = value;
            continue;
        }

        result.success = false;
        result.error = "unknown argument: " + argument;
        return result;
    }

    return result;
}

std::string webOptionsUsage()
{
    return
        "Usage: ethercat_diag_web [options]\n"
        "  --bind ADDRESS       IPv4 listen address (default 0.0.0.0)\n"
        "  --port PORT          TCP port 1..65535 (default 8080)\n"
        "  --data-dir PATH      directory containing diagnostic JSON files\n"
        "  --assets-dir PATH    directory containing Web assets\n"
        "  --help               show this help\n";
}
