#pragma once

#include "ethercat_diag/esc/esc_register_reader.h"

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
    EscDiagnosticReader();

    explicit EscDiagnosticReader(
        EscRegisterReader register_reader
    );

    EscDiagnosticSample read(
        int master_index,
        int slave_position
    ) const;

private:
    EscRegisterReader register_reader_;
};
