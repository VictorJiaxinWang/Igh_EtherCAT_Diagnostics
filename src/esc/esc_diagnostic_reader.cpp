#include "ethercat_diag/esc/esc_diagnostic_reader.h"

#include <utility>

bool EscDiagnosticSample::complete() const
{
    return dl_status.success
        && al_status.success
        && al_status_code.success;
}

EscDiagnosticReader::EscDiagnosticReader() = default;

EscDiagnosticReader::EscDiagnosticReader(
    EscRegisterReader register_reader
)
    : register_reader_(std::move(register_reader))
{
}

EscDiagnosticSample EscDiagnosticReader::read(
    int master_index,
    int slave_position
) const
{
    EscDiagnosticSample sample;

    sample.master_index = master_index;
    sample.slave_position = slave_position;

    sample.dl_status =
        register_reader_.readU16(
            master_index,
            slave_position,
            0x0110
        );

    sample.al_status =
        register_reader_.readU16(
            master_index,
            slave_position,
            0x0130
        );

    sample.al_status_code =
        register_reader_.readU16(
            master_index,
            slave_position,
            0x0134
        );

    return sample;
}
