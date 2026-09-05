#include "ethercat_diag/root_cause/root_cause_analyzer.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <string>
#include <vector>

namespace
{

bool containsText(
    const std::vector<std::string>& values,
    const std::string& expected)
{
    return std::any_of(
        values.begin(),
        values.end(),
        [&](const std::string& value) {
            return value.find(expected) != std::string::npos;
        });
}

DiagnosisEvidence baseEvidence(std::uint64_t timestamp, bool link_up)
{
    DiagnosisEvidence evidence;
    evidence.timestamp_ms = timestamp;
    evidence.master_link = MasterLinkEvidence{link_up};
    return evidence;
}

void testMasterLinkFailureWinsWhenLinkIsDown()
{
    DiagnosisEvidence evidence = baseEvidence(1000U, false);
    evidence.lost_slaves.push_back({0});

    const RootCauseReport report =
        RootCauseAnalyzer{}.analyze({evidence});

    assert(report.primary.kind ==
           RootCauseKind::MASTER_LINK_FAILURE);
    assert(report.primary.score >= 100);
    assert(report.conclusive);
    assert(report.confidence >= 0.70);
    assert(containsText(
        report.primary.supporting_evidence,
        "Master link is DOWN"));
}

void testBoundaryLinkFailureUsesBoundaryAndLostLinkIncrement()
{
    DiagnosisEvidence evidence = baseEvidence(2000U, true);
    evidence.boundary = BoundaryEvidence{1, 2};
    evidence.lost_slaves.push_back({2});
    evidence.port_errors.push_back({
        PortErrorEvidenceKind::LOST_LINK,
        1,
        0,
        3,
        4,
        1});

    const RootCauseReport report =
        RootCauseAnalyzer{}.analyze({evidence});

    assert(report.primary.kind ==
           RootCauseKind::BOUNDARY_LINK_FAILURE);
    assert(report.boundary.has_value());
    assert(report.boundary->last_alive_slave == 1);
    assert(report.boundary->first_lost_slave == 2);
    assert(report.conclusive);
    assert(containsText(
        report.primary.supporting_evidence,
        "Slave1 Port0 lost-link counter increased"));
}

void testSlaveInternalErrorOutranksBoundaryClues()
{
    DiagnosisEvidence evidence = baseEvidence(3000U, true);
    evidence.boundary = BoundaryEvidence{1, 2};
    evidence.lost_slaves.push_back({2});
    evidence.slave_states.push_back({
        1,
        AlState::SAFEOP,
        true,
        true});

    AlEvidence al;
    al.slave_position = 1;
    al.error_indication = true;
    al.warning_indication = false;
    al.status_code = 0x001BU;
    evidence.al_status = al;

    const RootCauseReport report =
        RootCauseAnalyzer{}.analyze({evidence});

    assert(report.primary.kind ==
           RootCauseKind::SLAVE_INTERNAL_ERROR);
    assert(report.conclusive);
    assert(containsText(
        report.primary.supporting_evidence,
        "AL Status Code=0x001b"));
    assert(!report.primary.recommended_actions.empty());
}

void testPortErrorsProduceNonConclusiveQualityDiagnosis()
{
    DiagnosisEvidence evidence = baseEvidence(4000U, true);
    evidence.port_errors.push_back({
        PortErrorEvidenceKind::RX_ERROR,
        4,
        1,
        10,
        14,
        4});

    const RootCauseReport report =
        RootCauseAnalyzer{}.analyze({evidence});

    assert(report.primary.kind ==
           RootCauseKind::LINK_QUALITY_DEGRADATION);
    assert(!report.conclusive);
    assert(report.primary.score > 0);
    assert(containsText(
        report.primary.supporting_evidence,
        "receive-error counter increased"));
}

void testMissingFaultEvidenceReturnsUnknown()
{
    DiagnosisEvidence evidence = baseEvidence(5000U, true);

    const RootCauseReport report =
        RootCauseAnalyzer{}.analyze({evidence});

    assert(report.primary.kind == RootCauseKind::UNKNOWN);
    assert(!report.conclusive);
    assert(report.primary.score == 0);
    assert(report.confidence == 0.0);
}

void testLatestMasterStateAndCollectionErrorsArePreserved()
{
    DiagnosisEvidence older = baseEvidence(6000U, false);
    DiagnosisEvidence newer = baseEvidence(7000U, true);
    newer.collection_errors.push_back("AL Status: timed out");
    newer.port_errors.push_back({
        PortErrorEvidenceKind::INVALID_FRAME,
        0,
        0,
        2,
        3,
        1});

    const RootCauseReport report =
        RootCauseAnalyzer{}.analyze({older, newer});

    assert(report.primary.kind !=
           RootCauseKind::MASTER_LINK_FAILURE);
    assert(containsText(
        report.collection_errors,
        "AL Status: timed out"));
}

void testLaterBoundaryCorrelatesEarlierPortEvidence()
{
    DiagnosisEvidence earlier = baseEvidence(7500U, true);
    earlier.port_errors.push_back({
        PortErrorEvidenceKind::LOST_LINK,
        1,
        0,
        0,
        1,
        1});

    DiagnosisEvidence later = baseEvidence(8000U, true);
    later.boundary = BoundaryEvidence{1, 2};
    later.lost_slaves.push_back({2});

    const RootCauseReport report =
        RootCauseAnalyzer{}.analyze({earlier, later});

    assert(report.primary.kind ==
           RootCauseKind::BOUNDARY_LINK_FAILURE);
    assert(containsText(
        report.primary.supporting_evidence,
        "Slave1 Port0 lost-link counter increased"));
}

void testEmptyWindowReturnsUnknownSafely()
{
    const RootCauseReport report =
        RootCauseAnalyzer{}.analyze({});

    assert(report.primary.kind == RootCauseKind::UNKNOWN);
    assert(report.timestamp_ms == 0U);
    assert(report.candidates.size() == 5U);
}

void testCloseCompetingScoresAreMarkedInconclusive()
{
    DiagnosisEvidence evidence = baseEvidence(9000U, true);
    evidence.boundary = BoundaryEvidence{1, 2};
    evidence.lost_slaves.push_back({2});
    AlEvidence al;
    al.slave_position = 1;
    al.status_code = 0x001BU;
    evidence.al_status = al;

    const RootCauseReport report =
        RootCauseAnalyzer{}.analyze({evidence});

    assert(report.primary.kind ==
           RootCauseKind::SLAVE_INTERNAL_ERROR);
    assert(!report.conclusive);
    assert(report.candidates[0].score -
           report.candidates[1].score < 15);
}

} // namespace

int main()
{
    testMasterLinkFailureWinsWhenLinkIsDown();
    testBoundaryLinkFailureUsesBoundaryAndLostLinkIncrement();
    testSlaveInternalErrorOutranksBoundaryClues();
    testPortErrorsProduceNonConclusiveQualityDiagnosis();
    testMissingFaultEvidenceReturnsUnknown();
    testLatestMasterStateAndCollectionErrorsArePreserved();
    testLaterBoundaryCorrelatesEarlierPortEvidence();
    testEmptyWindowReturnsUnknownSafely();
    testCloseCompetingScoresAreMarkedInconclusive();
    return 0;
}
