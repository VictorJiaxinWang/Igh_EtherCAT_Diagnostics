#include "ethercat_diag/esc/port_error_reader.h"

#include <utility>

PortErrorReader::PortErrorReader()
    : PortErrorReader(IoctlEscRegisterReader{})
{
}

PortErrorReader::PortErrorReader(
    IoctlEscRegisterReader register_reader)
    : read_block_(
          [reader = std::move(register_reader)](
              int master_index,
              int slave_position,
              std::uint16_t address,
              std::size_t size) {
              return reader.readBlock(
                  master_index,
                  slave_position,
                  address,
                  size);
          })
{
}

PortErrorReader::PortErrorReader(
    BlockReadFunction read_block)
    : read_block_(std::move(read_block))
{
}

PortErrorReadResult PortErrorReader::read(
    int master_index,
    int slave_position) const
{
    if (!read_block_)
    {
        return {false, {}, "port error reader is not configured"};
    }

    const RegisterBlockReadResult block =
        read_block_(
            master_index,
            slave_position,
            esc_port_error_base_address,
            esc_port_error_block_size);

    if (!block.success)
    {
        return {false, {}, block.error};
    }

    const PortErrorDecodeResult decoded =
        decodePortErrorCounters(block.bytes);

    if (!decoded.success)
    {
        return {false, {}, decoded.error};
    }

    return {true, decoded.counters, {}};
}
