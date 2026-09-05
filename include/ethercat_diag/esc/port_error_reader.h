#pragma once

#include "ethercat_diag/esc/ioctl_esc_register_reader.h"
#include "ethercat_diag/esc/port_error_decoder.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

struct PortErrorReadResult
{
    bool success{};
    PortErrorCounters counters;
    std::string error;
};

class PortErrorReader
{
public:
    using BlockReadFunction =
        std::function<RegisterBlockReadResult(
            int,
            int,
            std::uint16_t,
            std::size_t)>;

    PortErrorReader();

    explicit PortErrorReader(
        IoctlEscRegisterReader register_reader);

    explicit PortErrorReader(
        BlockReadFunction read_block);

    PortErrorReadResult read(
        int master_index,
        int slave_position) const;

private:
    BlockReadFunction read_block_;
};
