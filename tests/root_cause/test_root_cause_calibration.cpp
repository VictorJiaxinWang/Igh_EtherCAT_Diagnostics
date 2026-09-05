#include "ethercat_diag/root_cause/root_cause_calibration.h"

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

DiagnosisEvidence masterLinkDownEvidence()
{
    DiagnosisEvidence evidence;
    evidence.timestamp_ms = 1000U;
    evidence.master_link = MasterLinkEvidence{false};
    return evidence;
}

DiagnosisEvidence boundaryEvidence()
{
    DiagnosisEvidence evidence;
    evidence.timestamp_ms = 2000U;
    evidence.master_link = MasterLinkEvidence{true};
    evidence.boundary = BoundaryEvidence{1, 2};
    evidence.lost_slaves.push_back({2});
    evidence.port_errors.push_back({
        PortErrorEvidenceKind::LOST_LINK,
        1,
        0,
        0,
        1,
        1});
    return evidence;
}

DiagnosisEvidence weakQualityEvidence()
{
    DiagnosisEvidence evidence;
    evidence.timestamp_ms = 3000U;
    evidence.master_link = MasterLinkEvidence{true};
    evidence.port_errors.push_back({
        PortErrorEvidenceKind::RX_ERROR,
        1,
        0,
        8,
        9,
        1});
    return evidence;
}

void testDefaultWeightsPreserveBaselineDecision()
{
    const RootCauseReport report =
        RootCauseAnalyzer{}.analyze({masterLinkDownEvidence()});

    assert(report.primary.kind ==
           RootCauseKind::MASTER_LINK_FAILURE);
    assert(report.primary.score == 100);
    assert(report.conclusive);
}

void testInjectedWeightsCanDisableOneRuleForCalibration()
{
    RootCauseRuleWeights weights;
    weights.master_link_down = 0;

    const RootCauseReport report =
        RootCauseAnalyzer{weights}.analyze({masterLinkDownEvidence()});

    assert(report.primary.kind == RootCauseKind::UNKNOWN);
    assert(!report.conclusive);
}

void testNegativeWeightIsRejected()
{
    RootCauseRuleWeights weights;
    weights.al_status_code_error = -1;

    bool threw = false;
    try
    {
        RootCauseAnalyzer analyzer(weights);
        (void) analyzer;
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    assert(threw);
}

void testZeroWeightFullyDisablesQualityObservation()
{
    RootCauseRuleWeights weights;
    weights.quality_error_per_delta = 0;

    const RootCauseReport report =
        RootCauseAnalyzer{weights}.analyze({weakQualityEvidence()});

    assert(report.primary.kind == RootCauseKind::UNKNOWN);
    assert(report.primary.score == 0);
}

void testFaultMatrixCountsOnlyConclusiveMatchesAsPassed()
{
    const std::vector<FaultMatrixCase> cases = {
        {"master cable removed",
         RootCauseKind::MASTER_LINK_FAILURE,
         {masterLinkDownEvidence()}},
        {"cable between Slave1 and Slave2 removed",
         RootCauseKind::BOUNDARY_LINK_FAILURE,
         {boundaryEvidence()}},
        {"single receive error",
         RootCauseKind::LINK_QUALITY_DEGRADATION,
         {weakQualityEvidence()}}};

    const FaultMatrixSummary summary =
        RootCauseCalibration{}.evaluate(cases);

    assert(summary.entries.size() == 3U);
    assert(summary.passed == 2U);
    assert(std::fabs(summary.accuracy() - (2.0 / 3.0)) < 0.0001);
    assert(summary.entries[0].passed);
    assert(summary.entries[1].passed);
    assert(!summary.entries[2].passed);
    assert(summary.entries[2].actual ==
           RootCauseKind::LINK_QUALITY_DEGRADATION);
    assert(!summary.entries[2].conclusive);
}

void testUnknownExpectationDoesNotRequireConclusiveReport()
{
    DiagnosisEvidence healthy;
    healthy.timestamp_ms = 4000U;
    healthy.master_link = MasterLinkEvidence{true};

    const FaultMatrixSummary summary =
        RootCauseCalibration{}.evaluate({
            {"healthy baseline", RootCauseKind::UNKNOWN, {healthy}}});

    assert(summary.passed == 1U);
    assert(summary.entries[0].passed);
}

void testMatrixFormatterShowsExpectedActualAndAccuracy()
{
    const FaultMatrixSummary summary =
        RootCauseCalibration{}.evaluate({
            {"master cable removed",
             RootCauseKind::MASTER_LINK_FAILURE,
             {masterLinkDownEvidence()}},
            {"weak quality",
             RootCauseKind::LINK_QUALITY_DEGRADATION,
             {weakQualityEvidence()}}});

    const std::string output = formatFaultMatrixSummary(summary);
    assert(output.find("accuracy=50.0%") != std::string::npos);
    assert(output.find("expected=MASTER_LINK_FAILURE") !=
           std::string::npos);
    assert(output.find("actual=LINK_QUALITY_DEGRADATION") !=
           std::string::npos);
    assert(output.find("INCONCLUSIVE") != std::string::npos);
}

} // namespace

int main()
{
    testDefaultWeightsPreserveBaselineDecision();
    testInjectedWeightsCanDisableOneRuleForCalibration();
    testNegativeWeightIsRejected();
    testZeroWeightFullyDisablesQualityObservation();
    testFaultMatrixCountsOnlyConclusiveMatchesAsPassed();
    testUnknownExpectationDoesNotRequireConclusiveReport();
    testMatrixFormatterShowsExpectedActualAndAccuracy();
    return 0;
}
