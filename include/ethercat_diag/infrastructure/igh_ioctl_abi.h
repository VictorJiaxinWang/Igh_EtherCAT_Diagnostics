#pragma once

#include "master/ioctl.h"

#include <cstdint>

namespace igh_ioctl_abi
{

// The MagicLab RK3588 module reports 1.6.3 but deliberately retains
// ioctl magic 32. Its DWARF type layout and request numbers match the
// imported upstream 1.6.3 structures used by this project.
inline constexpr std::uint32_t version_magic = 32U;

} // namespace igh_ioctl_abi
