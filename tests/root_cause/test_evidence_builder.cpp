#include "ethercat_diag/root_cause/evidence_builder.h"

#include <cassert>
#include <iostream>
#include <optional>
#include <vector>

namespace
{

NetworkSnapshot makeSnapshot()
{
    NetworkSnapshot snapshot;
    snapshot.master.timestamp_ms = 5000U;
    snapshot.master.master_index = 0;
    snapshot.master.link_up = true;
    snapshot.master.slave_count = 2;
    snapshot.slaves.push_back(
        SlaveSnapshot{0, 0, 0, AlState::OP,
                      false, true, "coupler"});
    snapshot.slaves.push_back(
        SlaveSnapshot{1, 0, 1, AlState::SAFEOP,
                      true, true, "drive"});
    return snapshot;
}

void testBuildsTypedFactsFromSnapshotAndEvents()
{
    const NetworkSnapshot snapshot = makeSnapshot();
    const std::vector<FaultEvent> events{
        {5000U, EventType::SLAVE_LOST, 2, 1, 0,
         "Slave 2 was lost"},
        {5000U, EventType::PORT_INVALID_FRAME_INCREASED,
         1, 3, 5, "invalid increased", 0},
        {5000U, EventType::PORT_RX_ERROR_INCREASED,
         1, 4, 7, "rx increased", 1},
        {5000U, EventType::PORT_FORWARDED_RX_ERROR_INCREASED,
         1, 8, 9, "forwarded increased", 2},
        {5000U, EventType::PORT_LOST_LINK_INCREASED,
         1, 10, 12, "lost link increased", 3}
    };

    const DiagnosisEvidence evidence =
        EvidenceBuilder{}.build(snapshot, events, std::nullopt);

    assert(evidence.timestamp_ms == 5000U);
    assert(evidence.master_link.has_value());
    assert(evidence.master_link->link_up);
    assert(evidence.lost_slaves.size() == 1U);
    assert(evidence.lost_slaves[0].slave_position == 2);
    assert(evidence.slave_states.size() == 2U);
    assert(!evidence.slave_states[0].has_error);
    assert(evidence.slave_states[1].has_error);

    assert(evidence.port_errors.size() == 4U);
    assert(evidence.port_errors[0].kind ==
           PortErrorEvidenceKind::INVALID_FRAME);
    assert(evidence.port_errors[1].kind ==
           PortErrorEvidenceKind::RX_ERROR);
    assert(evidence.port_errors[2].kind ==
           PortErrorEvidenceKind::FORWARDED_RX_ERROR);
    assert(evidence.port_errors[3].kind ==
           PortErrorEvidenceKind::LOST_LINK);
    assert(evidence.port_errors[0].slave_position == 1);
    assert(evidence.port_errors[0].port_position == 0);
    assert(evidence.port_errors[0].old_value == 3);
    assert(evidence.port_errors[0].new_value == 5);
    assert(evidence.port_errors[0].delta == 2);
    assert(!evidence.al_status.has_value());
    assert(!evidence.boundary.has_value());
}

void testBuildsBoundaryAndActiveEscEvidence()
{
    DiagResult diagnosis;
    diagnosis.master_index = 0;
    diagnosis.boundary = {1, 2, true};

    EscDiagnosticSample sample;
    sample.master_index = 0;
    sample.slave_position = 1;
    sample.dl_status = {true, 0x5613U, {}};
    sample.al_status = {true, 0x0012U, {}};
    sample.al_status_code = {true, 0x001BU, {}};
    diagnosis.sample = sample;

    PortErrorCounters raw_counters;
    raw_counters.ports[0].lost_link = 9U;
    diagnosis.port_errors =
        PortErrorReadResult{true, raw_counters, {}};

    const DiagnosisEvidence evidence = EvidenceBuilder{}.build(
        makeSnapshot(), {}, diagnosis);

    assert(evidence.boundary.has_value());
    assert(evidence.boundary->last_alive_slave == 1);
    assert(evidence.boundary->first_lost_slave == 2);
    assert(evidence.al_status.has_value());
    assert(evidence.al_status->slave_position == 1);
    assert(evidence.al_status->raw_status == 0x0012U);
    assert(evidence.al_status->state == EscAlState::PreOp);
    assert(evidence.al_status->error_indication);
    assert(evidence.al_status->warning_indication == false);
    assert(evidence.al_status->status_code == 0x001BU);
    assert(evidence.port_snapshot.has_value());
    assert(evidence.port_snapshot->counters.ports[0].lost_link == 9U);
    assert(evidence.port_errors.empty());
    assert(evidence.collection_errors.empty());
}

void testPartialReadsRemainUnavailableAndKeepErrors()
{
    DiagResult diagnosis;
    diagnosis.boundary = {1, 2, true};

    EscDiagnosticSample sample;
    sample.slave_position = 1;
    sample.al_status = {false, 0U, "AL status unavailable"};
    sample.al_status_code = {true, 0x0000U, {}};
    diagnosis.sample = sample;
    diagnosis.port_errors =
        PortErrorReadResult{false, {}, "port counters unavailable"};

    const DiagnosisEvidence evidence = EvidenceBuilder{}.build(
        makeSnapshot(), {}, diagnosis);

    assert(evidence.al_status.has_value());
    assert(!evidence.al_status->raw_status.has_value());
    assert(!evidence.al_status->state.has_value());
    assert(evidence.al_status->status_code == 0x0000U);
    assert(!evidence.port_snapshot.has_value());
    assert(evidence.collection_errors.size() == 2U);
}

void testRejectsNonIncreasingPortEventAsInvalidEvidence()
{
    const std::vector<FaultEvent> events{
        {5000U, EventType::PORT_RX_ERROR_INCREASED,
         1, 7, 7, "not actually increased", 0}
    };

    const DiagnosisEvidence evidence = EvidenceBuilder{}.build(
        makeSnapshot(), events, std::nullopt);

    assert(evidence.port_errors.empty());
    assert(evidence.collection_errors.size() == 1U);
}

} // namespace

int main()
{
    testBuildsTypedFactsFromSnapshotAndEvents();
    testBuildsBoundaryAndActiveEscEvidence();
    testPartialReadsRemainUnavailableAndKeepErrors();
    testRejectsNonIncreasingPortEventAsInvalidEvidence();
    std::cout << "evidence builder tests passed\n";
}
