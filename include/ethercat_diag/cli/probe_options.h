#pragma once

#include <optional>
#include <string>
#include <vector>

struct ProbeOptions
{
    int master_index{};
    int slave_position{};
};

std::optional<ProbeOptions> parseProbeOptions(
    const std::vector<std::string>& arguments
);
