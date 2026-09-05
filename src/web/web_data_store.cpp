#include "ethercat_diag/web/web_data_store.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <utility>
#include <vector>

namespace
{

bool hasNonWhitespace(const std::string& value)
{
    return value.find_first_not_of(" \t\r\n") != std::string::npos;
}

std::string trim(const std::string& value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return {};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1U);
}

bool looksLikeCompleteObject(const std::string& value)
{
    return value.size() >= 2U &&
        value.front() == '{' &&
        value.back() == '}';
}

}

WebDataStore::WebDataStore(std::filesystem::path directory)
    : directory_(std::move(directory))
{
}

WebDataResult WebDataStore::readStatus() const
{
    std::ifstream input(
        directory_ / "latest_status.json",
        std::ios::binary);
    if (!input)
    {
        return {false, {}, "diagnostic status is unavailable"};
    }

    const std::string body{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};

    if (input.bad() || !hasNonWhitespace(body))
    {
        return {false, {}, "diagnostic status is unavailable"};
    }

    return {true, body, {}};
}

WebDataResult WebDataStore::readRecentEvents(std::size_t limit) const
{
    const std::filesystem::path path = directory_ / "events.jsonl";
    std::error_code filesystem_error;
    if (!std::filesystem::exists(path, filesystem_error))
    {
        if (filesystem_error)
        {
            return {false, {}, "diagnostic events are unavailable"};
        }
        return {true, "[]\n", {}};
    }

    std::ifstream input(path);
    if (!input)
    {
        return {false, {}, "diagnostic events are unavailable"};
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line))
    {
        line = trim(line);
        if (looksLikeCompleteObject(line))
        {
            lines.push_back(std::move(line));
        }
    }

    if (!input.eof())
    {
        return {false, {}, "diagnostic events are unavailable"};
    }

    const std::size_t count = std::min(limit, lines.size());
    const std::size_t first = lines.size() - count;
    std::ostringstream output;
    output << '[';
    for (std::size_t index = first; index < lines.size(); ++index)
    {
        if (index != first)
        {
            output << ',';
        }
        output << lines[index];
    }
    output << "]\n";
    return {true, output.str(), {}};
}
