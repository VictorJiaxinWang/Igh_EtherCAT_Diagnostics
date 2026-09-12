#include "ethercat_diag/cli/daemon_options.h"

#include <charconv>
#include <set>

std::vector<int> parseMasterList(const std::string& text)
{
    std::vector<int> result;
    std::set<int> unique;
    std::size_t begin = 0U;
    while (begin < text.size())
    {
        const std::size_t end = text.find(',', begin);
        const std::string token = text.substr(
            begin, end == std::string::npos ? end : end - begin);
        int value = -1;
        const auto parsed = std::from_chars(
            token.data(), token.data() + token.size(), value);
        if (token.empty() || parsed.ec != std::errc{} ||
            parsed.ptr != token.data() + token.size() ||
            value < 0 || !unique.insert(value).second)
        {
            return {};
        }
        result.push_back(value);
        if (end == std::string::npos)
        {
            break;
        }
        begin = end + 1U;
    }
    return result;
}
