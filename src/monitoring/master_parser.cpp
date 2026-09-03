#include "ethercat_diag/monitoring/master_parser.h"

#include <sstream>
#include <string>

namespace
{

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

MasterSnapshot parsed;

bool parseMasterOutput(
    const std::string& output,
    MasterSnapshot& snapshot)
{
    bool has_phase = false;
    bool has_active = false;
    bool has_link = false;
    bool has_slave_count = false;

    std::istringstream input_stream(output);
    std::string line;

    while (std::getline(input_stream, line))
    {
        line = trim(line);

        if (line.rfind("Phase:", 0) == 0)
        {
            parsed.phase = trim(line.substr(6));
            has_phase = !parsed.phase.empty();
        }
        else if (line.rfind("Active:", 0) == 0)
        {
            const std::string value =
                trim(line.substr(7));

            if (value == "yes")
            {
                parsed.active = true;
            }
            else if (value == "no")
            {
                parsed.active = false;
            }
            else
            {
                return false;
            }

            has_active = true;
        }
        else if (line.rfind("Slaves:", 0) == 0)
        {
            const std::string value =
                trim(line.substr(7));

            std::istringstream number_stream(value);
            int count = -1;

            number_stream >> count;
            number_stream >> std::ws;

            if (!number_stream.eof() || count < 0)
            {
                return false;
            }

            parsed.slave_count = count;
            has_slave_count = true;
        }
        else if (line.rfind("Link:", 0) == 0)
        {
            const std::string value =
                trim(line.substr(5));

            if (value == "UP")
            {
                parsed.link_up = true;
            }
            else if (value == "DOWN")
            {
                parsed.link_up = false;
            }
            else
            {
                return false;
            }

            has_link = true;
        }
    }

    const bool complete = has_phase && has_active && has_link && has_slave_count;

    if (!complete)
    {
	    return false;
    }

    snapshot = parsed;
    return true;
}
