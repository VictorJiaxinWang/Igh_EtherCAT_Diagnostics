#include "ethercat_diag/diagnosis/active_diagnosis.h"

#include <utility>

bool DiagResult::attempted() const
{
    return sample.has_value();
}

bool DiagResult::success() const
{
    return sample.has_value() &&
        sample->complete() &&
        (!port_errors.has_value() || port_errors->success);
}

ActiveDiagnosis::ActiveDiagnosis(int master_index)
    : master_index_(master_index),
      port_error_reader_(PortErrorReader{})
{
}

ActiveDiagnosis::ActiveDiagnosis(
    int master_index,
    EscDiagnosticReader reader,
    PortErrorReader port_error_reader)
    : master_index_(master_index),
      reader_(std::move(reader)),
      port_error_reader_(std::move(port_error_reader))
{
}

ActiveDiagnosis::ActiveDiagnosis(
    int master_index,
    EscDiagnosticReader reader
)
    : master_index_(master_index),
      reader_(std::move(reader))
{
}

DiagResult ActiveDiagnosis::run(
    const FaultBoundary& boundary) const
{
    DiagResult result;
    result.master_index = master_index_;
    result.boundary = boundary;

    if (master_index_ < 0)
    {
        result.error = "Negative master index";
        return result;
    }

    if (!boundary.valid)
    {
        result.error = "Invalid fault boundary";
        return result;
    }

    if (boundary.last_alive_slave < 0 ||
        boundary.first_lost_slave < 0)
    {
        result.error = "Negative slave position";
        return result;
    }

    result.sample = reader_.read(
        master_index_,
        boundary.last_alive_slave);

    if (port_error_reader_)
    {
        result.port_errors = port_error_reader_->read(
            master_index_,
            boundary.last_alive_slave);
    }

    return result;
}
