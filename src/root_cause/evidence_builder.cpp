#include "ethercat_diag/root_cause/evidence_builder.h"

namespace
{

std::optional<PortErrorEvidenceKind> portEvidenceKind(
    EventType type)
{
    switch (type)
    {
    case EventType::PORT_INVALID_FRAME_INCREASED:
        return PortErrorEvidenceKind::INVALID_FRAME;
    case EventType::PORT_RX_ERROR_INCREASED:
        return PortErrorEvidenceKind::RX_ERROR;
    case EventType::PORT_FORWARDED_RX_ERROR_INCREASED:
        return PortErrorEvidenceKind::FORWARDED_RX_ERROR;
    case EventType::PORT_LOST_LINK_INCREASED:
        return PortErrorEvidenceKind::LOST_LINK;
    default:
        return std::nullopt;
    }
}

void appendReadError(
    std::vector<std::string>& errors,
    const std::string& label,
    const RegisterReadResult& result)
{
    if (!result.success && !result.error.empty())
    {
        errors.push_back(label + ": " + result.error);
    }
}

} // namespace

DiagnosisEvidence EvidenceBuilder::build(
    const NetworkSnapshot& snapshot,
    const std::vector<FaultEvent>& events,
    const std::optional<DiagResult>& diagnosis) const
{
    DiagnosisEvidence evidence;
    evidence.timestamp_ms = snapshot.master.timestamp_ms;
    evidence.master_link =
        MasterLinkEvidence{snapshot.master.link_up};

    for (const SlaveSnapshot& slave : snapshot.slaves)
    {
        evidence.slave_states.push_back(SlaveStateEvidence{
            slave.position,
            slave.state,
            slave.has_error,
            slave.online});
    }

    for (const FaultEvent& event : events)
    {
        if (event.type == EventType::SLAVE_LOST)
        {
            evidence.lost_slaves.push_back(
                SlaveLossEvidence{event.slave_position});
        }

        const auto kind = portEvidenceKind(event.type);
        if (!kind)
        {
            continue;
        }

        if (event.slave_position < 0 ||
            event.port_position < 0 ||
            event.new_value <= event.old_value)
        {
            evidence.collection_errors.push_back(
                "invalid port error increase event");
            continue;
        }

        evidence.port_errors.push_back(PortErrorEvidence{
            *kind,
            event.slave_position,
            event.port_position,
            event.old_value,
            event.new_value,
            event.new_value - event.old_value});
    }

    if (!diagnosis)
    {
        return evidence;
    }

    if (diagnosis->boundary.valid)
    {
        evidence.boundary = BoundaryEvidence{
            diagnosis->boundary.last_alive_slave,
            diagnosis->boundary.first_lost_slave,
            diagnosis->boundary.last_alive_alias,
            diagnosis->boundary.last_alive_relative_position,
            diagnosis->boundary.first_lost_alias,
            diagnosis->boundary.first_lost_relative_position};
    }

    if (!diagnosis->error.empty())
    {
        evidence.collection_errors.push_back(
            "active diagnosis: " + diagnosis->error);
    }

    if (diagnosis->sample)
    {
        const EscDiagnosticSample& sample = *diagnosis->sample;
        AlEvidence al;
        al.slave_position = sample.slave_position;

        if (sample.al_status.success)
        {
            const AlStatusInfo decoded =
                decodeAlStatus(sample.al_status.value);
            al.raw_status = sample.al_status.value;
            al.state = decoded.state;
            al.error_indication = decoded.error_indication;
            al.warning_indication = decoded.warning_indication;
        }

        if (sample.al_status_code.success)
        {
            al.status_code = sample.al_status_code.value;
        }

        evidence.al_status = al;
        appendReadError(
            evidence.collection_errors,
            "DL Status",
            sample.dl_status);
        appendReadError(
            evidence.collection_errors,
            "AL Status",
            sample.al_status);
        appendReadError(
            evidence.collection_errors,
            "AL Status Code",
            sample.al_status_code);
    }

    if (diagnosis->port_errors)
    {
        if (diagnosis->port_errors->success)
        {
            int slave_position = -1;
            if (diagnosis->sample)
            {
                slave_position = diagnosis->sample->slave_position;
            }
            evidence.port_snapshot = PortCounterSnapshotEvidence{
                slave_position,
                diagnosis->port_errors->counters};
        }
        else if (!diagnosis->port_errors->error.empty())
        {
            evidence.collection_errors.push_back(
                "Port Error Counters: " +
                diagnosis->port_errors->error);
        }
    }

    return evidence;
}
