#include "ethercat_diag/esc/esc_diagnostic_reader.h"

#include <utility>

bool EscDiagnosticSample::complete() const
{
    return dl_status.success
        && al_status.success
        && al_status_code.success;
}

EscDiagnosticReader::EscDiagnosticReader()
    : EscDiagnosticReader(IoctlEscRegisterReader{})
{
}

EscDiagnosticReader::EscDiagnosticReader(
    IoctlEscRegisterReader register_reader)
    : read_register_(
          [reader = std::move(register_reader)](
              int master_index,
              int slave_position,
              std::uint16_t address) {
              return reader.readU16(
                  master_index,
                  slave_position,
                  address);
          })
{
}

EscDiagnosticReader::EscDiagnosticReader(
    RegisterReadFunction read_register)
    : read_register_(std::move(read_register))
{
}

EscDiagnosticSample EscDiagnosticReader::read(
    int master_index,
    int slave_position) const
{
    EscDiagnosticSample sample;
    sample.master_index = master_index;
    sample.slave_position = slave_position;

    if (!read_register_)
    {
        const RegisterReadResult failure{
            false,
            0U,
            "ESC register read function is not configured"};
        sample.dl_status = failure;
        sample.al_status = failure;
        sample.al_status_code = failure;
        return sample;
    }

    sample.dl_status = read_register_(
        master_index,
        slave_position,
        0x0110U);

    sample.al_status = read_register_(
        master_index,
        slave_position,
        0x0130U);

    sample.al_status_code = read_register_(
        master_index,
        slave_position,
        0x0134U);

    return sample;
}
