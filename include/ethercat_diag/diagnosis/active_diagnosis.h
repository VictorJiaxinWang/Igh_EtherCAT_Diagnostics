#pragma once

#include "ethercat_diag/detection/boundary_locator.h"
#include "ethercat_diag/esc/esc_diagnostic_reader.h"
#include "ethercat_diag/esc/port_error_reader.h"

#include <optional>
#include <string>

struct DiagResult
{
    int master_index{-1};
    FaultBoundary boundary{};
    std::optional<EscDiagnosticSample> sample;
    std::optional<PortErrorReadResult> port_errors;
    std::string error;

    bool attempted() const;
    bool success() const;
};

class ActiveDiagnosis
{
public:
    explicit ActiveDiagnosis(int master_index = 0);

    ActiveDiagnosis(
        int master_index,
        EscDiagnosticReader reader
    );

    ActiveDiagnosis(
        int master_index,
        EscDiagnosticReader reader,
        PortErrorReader port_error_reader);

    DiagResult run(const FaultBoundary& boundary) const;

private:
    int master_index_{};
    EscDiagnosticReader reader_;
    std::optional<PortErrorReader> port_error_reader_;
};
