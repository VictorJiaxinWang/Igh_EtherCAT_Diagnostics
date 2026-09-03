#include "ethercat_diag/monitoring/slave_parser.h"

#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <charconv>
#include <system_error>

namespace
{

bool parseNonNegativeInt(
    const std::string& text,
    int& value)
{
    if (text.empty())
    {
        return false;
    }

    int parsed_value = 0;

    const char* begin = text.data();
    const char* end = begin + text.size();

    const auto result =
        std::from_chars(begin, end, parsed_value);

    if (result.ec != std::errc{})
    {
        return false;
    }

    if (result.ptr != end)
    {
        return false;
    }

    if (parsed_value < 0)
    {
        return false;
    }

    value = parsed_value;
    return true;
}

std::string trim(const std::string& text)
{
    const std::size_t first =
        text.find_first_not_of(" \t\r\n");

    if (first == std::string::npos)
    {
        return "";
    }

    const std::size_t last =
        text.find_last_not_of(" \t\r\n");

    return text.substr(first, last - first + 1);
}

}

AlState stringToAlState(std::string inputStr)
{
    if (inputStr == "INIT")
    {
        return AlState::INIT;
    }
    else if (inputStr == "PREOP")
    {
        return AlState::PREOP;
    }
    else if (inputStr == "SAFEOP")
    {
        return AlState::SAFEOP;
    }
    else if (inputStr == "OP")
    {
        return AlState::OP;
    }
    else if (inputStr == "UNKNOWN")
    {
        return AlState::UNKNOWN;
    }
    else
    {
        return AlState::UNKNOWN;
    }
}

bool parseSlavesOutput(const std::string& output, std::vector<SlaveSnapshot>& slaves)
{
    // static_cast<void>(output);
    // static_cast<void>(slaves);
    bool has_position = false;
    bool has_alias = false;
    bool has_relative_position = false;
    bool has_state = false;
    bool has_error_content = false;
    bool has_name = false;

    std::istringstream input_stream(output);
    std::vector<SlaveSnapshot> parsed;
    std::string line;
    std::string token;

    while(std::getline(input_stream, line))
    {
        line = trim(line);
        std::istringstream iss(line);
        std::string name;
        SlaveSnapshot slaveSnapshot;
        slaveSnapshot.online = true;
        uint8_t tokenSeq = 0;

        while(iss >> token)
        {
            switch(tokenSeq)
            {
                case 0: {
                    if (!parseNonNegativeInt(token, slaveSnapshot.position))
                    {
                        return false;
                    }
                    has_position = true;
                    break;
                }

                case 1: {
                    const std::size_t pos = token.find(':');
                    if (pos == std::string::npos || pos == 0
                        || pos + 1 >= token.size() || token.find(':', pos + 1) != std::string::npos)
                    {
                        std::cerr << "Can't find spliter ':'" << std::endl;
                        return false;
                    }
                    const std::string leftStr = token.substr(0, pos);
                    const std::string rightStr = token.substr(pos + 1);
                    if (!parseNonNegativeInt(
                            leftStr,
                            slaveSnapshot.alias))
                    {
                        return false;
                    }

                    if (!parseNonNegativeInt(
                            rightStr,
                            slaveSnapshot.relative_position))
                    {
                        return false;
                    }

                    has_alias = true;
                    has_relative_position = true;
                    break;
                }

                case 2: {
                    slaveSnapshot.state = stringToAlState(token);
                    has_state = true;
                    break;
                }

                case 3: {
                    if (token == "+")
                    {
                        slaveSnapshot.has_error = false;
                        has_error_content = true;
                    }
                    else if (token == "E")
                    {
                        slaveSnapshot.has_error = true;
                        has_error_content = true;
                    }
                    else
                    {
                        slaveSnapshot.has_error = true;
                        has_error_content = false;
                    }
                    break;
                }

                default:
                    break;
            }

            tokenSeq++;

            if (tokenSeq == 4)
            {
                std::getline(iss, name);
                name = trim(name);

                if (name.empty())
                {
                    return false;
                }
                
                has_name = true;
                slaveSnapshot.name = name;
            }
        }

        const bool complete = has_position && has_alias && has_relative_position && has_state && has_error_content && has_name;
        
        if (!complete)
        {
            return false;
        }

        parsed.push_back(slaveSnapshot);

        tokenSeq = 0;
        has_position = false;
        has_alias = false;
        has_relative_position = false;
        has_state = false;
        has_error_content = false;
        has_name = false;
    }

    slaves = parsed;
    return true;
}