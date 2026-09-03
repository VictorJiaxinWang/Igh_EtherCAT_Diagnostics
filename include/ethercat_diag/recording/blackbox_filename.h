#pragma once

#include <cstdint>
#include <filesystem>

std::filesystem::path makeUniqueBlackboxPath(
    const std::filesystem::path& directory,
    std::uint64_t timestamp_ms);
