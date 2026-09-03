#include "ethercat_diag/cli/probe_options.h"

#include <cassert>
#include <string>
#include <vector>
#include <iostream>

void testParsesValidProbeOptions()
{
    const std::optional<ProbeOptions> options =
        parseProbeOptions({"0", "3"});

    assert(options.has_value());
    assert(options->master_index == 0);
    assert(options->slave_position == 3);
}

void testRejectsInvalidProbeOptions()
{
    const std::vector<std::vector<std::string>>
        invalid_arguments{
            {},
            {"0"},
            {"0", "3", "extra"},
            {"-1", "3"},
            {"0", "-1"},
            {"master", "3"},
            {"0", "3abc"},
            {"0", "999999999999999999999"}
        };

    for (const auto& arguments : invalid_arguments)
    {
        const std::optional<ProbeOptions> options =
            parseProbeOptions(arguments);

        assert(!options.has_value());
    }
}

int main()
{
    testParsesValidProbeOptions();
    testRejectsInvalidProbeOptions();

    std::cout << "All ESC register reader tests passed\n";
    return 0;
}
