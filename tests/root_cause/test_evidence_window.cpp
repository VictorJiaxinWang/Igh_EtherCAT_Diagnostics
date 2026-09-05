#include "ethercat_diag/root_cause/evidence_window.h"

#include <cassert>
#include <iostream>
#include <stdexcept>

namespace
{

DiagnosisEvidence evidenceAt(std::uint64_t timestamp_ms)
{
    DiagnosisEvidence evidence;
    evidence.timestamp_ms = timestamp_ms;
    evidence.master_link = MasterLinkEvidence{true};
    return evidence;
}

void testKeepsTheInclusiveTenSecondBoundary()
{
    EvidenceWindow window(10000U);
    window.push(evidenceAt(1000U));
    window.push(evidenceAt(11000U));

    assert(window.recent().size() == 2U);

    window.push(evidenceAt(11001U));

    assert(window.recent().size() == 2U);
    assert(window.recent().front().timestamp_ms == 11000U);
    assert(window.recent().back().timestamp_ms == 11001U);
}

void testBackwardTimestampStartsANewWindow()
{
    EvidenceWindow window(10000U);
    window.push(evidenceAt(20000U));
    window.push(evidenceAt(9000U));

    assert(window.recent().size() == 1U);
    assert(window.recent().front().timestamp_ms == 9000U);
}

void testRejectsZeroDuration()
{
    bool threw = false;
    try
    {
        EvidenceWindow window(0U);
        (void)window;
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }

    assert(threw);
}

void testFiltersFactsToTheRequestedBoundary()
{
    EvidenceWindow window(10000U);
    DiagnosisEvidence evidence = evidenceAt(5000U);
    evidence.boundary = BoundaryEvidence{0, 1};
    evidence.lost_slaves = {{1}, {3}};
    evidence.port_errors = {
        {PortErrorEvidenceKind::RX_ERROR, 1, 0, 1, 2, 1},
        {PortErrorEvidenceKind::LOST_LINK, 2, 1, 4, 5, 1},
        {PortErrorEvidenceKind::INVALID_FRAME, 3, 0, 7, 8, 1}
    };
    evidence.slave_states = {
        {0, AlState::OP, false, true},
        {1, AlState::SAFEOP, true, true},
        {2, AlState::OP, false, true},
        {3, AlState::OP, false, true}
    };
    AlEvidence al;
    al.slave_position = 1;
    al.status_code = 0x001BU;
    evidence.al_status = al;
    PortCounterSnapshotEvidence snapshot;
    snapshot.slave_position = 2;
    evidence.port_snapshot = snapshot;
    window.push(evidence);

    const auto related =
        window.relatedToBoundary(BoundaryEvidence{2, 3});

    assert(related.size() == 1U);
    assert(related[0].master_link.has_value());
    assert(!related[0].boundary.has_value());
    assert(related[0].lost_slaves.size() == 1U);
    assert(related[0].lost_slaves[0].slave_position == 3);
    assert(related[0].port_errors.size() == 2U);
    assert(related[0].port_errors[0].slave_position == 2);
    assert(related[0].port_errors[1].slave_position == 3);
    assert(related[0].slave_states.size() == 2U);
    assert(related[0].slave_states[0].slave_position == 2);
    assert(related[0].slave_states[1].slave_position == 3);
    assert(!related[0].al_status.has_value());
    assert(related[0].port_snapshot.has_value());
    assert(related[0].port_snapshot->slave_position == 2);
}

void testKeepsMatchingBoundaryAndLastAliveAlEvidence()
{
    EvidenceWindow window(10000U);
    DiagnosisEvidence evidence = evidenceAt(5000U);
    evidence.boundary = BoundaryEvidence{2, 3};
    AlEvidence al;
    al.slave_position = 2;
    al.status_code = 0U;
    evidence.al_status = al;
    window.push(evidence);

    const auto related =
        window.relatedToBoundary(BoundaryEvidence{2, 3});

    assert(related[0].boundary.has_value());
    assert(related[0].al_status.has_value());
}

} // namespace

int main()
{
    testKeepsTheInclusiveTenSecondBoundary();
    testBackwardTimestampStartsANewWindow();
    testRejectsZeroDuration();
    testFiltersFactsToTheRequestedBoundary();
    testKeepsMatchingBoundaryAndLastAliveAlEvidence();
    std::cout << "evidence window tests passed\n";
}
