#include "ethercat_diag/diagnosis/active_diagnosis.h"

#include <utility>

bool DiagResult::attempted() const
{
    return sample.has_value();
}

bool DiagResult::success() const
{
    return sample.has_value() && sample->complete();
}

ActiveDiagnosis::ActiveDiagnosis(int master_index)
    : master_index_(master_index)
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

    return result;
}

