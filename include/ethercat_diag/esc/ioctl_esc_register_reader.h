#pragma once

#include "ethercat_diag/esc/esc_register_reader.h"
#include "ethercat_diag/infrastructure/igh_master_device.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

struct RegisterBlockReadResult
{
    bool success{};
    std::vector<std::uint8_t> bytes;
    std::string error;
};

class IoctlEscRegisterReader
{
public:
    explicit IoctlEscRegisterReader(
        LinuxCalls calls = productionLinuxCalls());

    RegisterReadResult readU16(
        int master_index,
        int slave_position,
        std::uint16_t address) const;

    RegisterBlockReadResult readBlock(
        int master_index,
        int slave_position,
        std::uint16_t address,
        std::size_t size) const;

private:
    LinuxCalls calls_;
};
