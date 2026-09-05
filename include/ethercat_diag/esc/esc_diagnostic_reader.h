#pragma once

#include "ethercat_diag/esc/esc_register_reader.h"
#include "ethercat_diag/esc/ioctl_esc_register_reader.h"

#include <cstdint>
#include <functional>

struct EscDiagnosticSample
{
    int master_index{-1};
    int slave_position{-1};

    RegisterReadResult dl_status;
    RegisterReadResult al_status;
    RegisterReadResult al_status_code;

    bool complete() const;
};

class EscDiagnosticReader
{
public:
    using RegisterReadFunction = std::function<RegisterReadResult(
        int,
        int,
        std::uint16_t)>;

    EscDiagnosticReader();

    explicit EscDiagnosticReader(
        EscRegisterReader register_reader);

    explicit EscDiagnosticReader(
        IoctlEscRegisterReader register_reader);

    explicit EscDiagnosticReader(
        RegisterReadFunction read_register);

    EscDiagnosticSample read(
        int master_index,
        int slave_position) const;

private:
    RegisterReadFunction read_register_;
};
