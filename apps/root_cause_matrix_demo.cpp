#include "ethercat_diag/root_cause/root_cause_calibration.h"

#include <iostream>
#include <vector>

namespace
{

DiagnosisEvidence masterFailure()
{
    DiagnosisEvidence evidence;
    evidence.timestamp_ms = 1000U;
    evidence.master_link = MasterLinkEvidence{false};
    return evidence;
}

DiagnosisEvidence boundaryFailure()
{
    DiagnosisEvidence evidence;
    evidence.timestamp_ms = 2000U;
    evidence.master_link = MasterLinkEvidence{true};
    evidence.boundary = BoundaryEvidence{1, 2};
    evidence.lost_slaves.push_back({2});
    evidence.port_errors.push_back({
        PortErrorEvidenceKind::LOST_LINK, 1, 0, 0, 1, 1});
    return evidence;
}

DiagnosisEvidence slaveFailure()
{
    DiagnosisEvidence evidence;
    evidence.timestamp_ms = 3000U;
    evidence.master_link = MasterLinkEvidence{true};
    AlEvidence al;
    al.slave_position = 1;
    al.error_indication = true;
    al.status_code = 0x001BU;
    evidence.al_status = al;
    return evidence;
}

DiagnosisEvidence qualityFailure()
{
    DiagnosisEvidence evidence;
    evidence.timestamp_ms = 4000U;
    evidence.master_link = MasterLinkEvidence{true};
    evidence.port_errors.push_back({
        PortErrorEvidenceKind::RX_ERROR, 1, 0, 0, 6, 6});
    evidence.port_errors.push_back({
        PortErrorEvidenceKind::INVALID_FRAME, 1, 0, 0, 6, 6});
    return evidence;
}

} // namespace

int main()
{
    const std::vector<FaultMatrixCase> cases = {
        {"master cable removed",
         RootCauseKind::MASTER_LINK_FAILURE,
         {masterFailure()}},
        {"cable between Slave1 and Slave2 removed",
         RootCauseKind::BOUNDARY_LINK_FAILURE,
         {boundaryFailure()}},
        {"slave AL error",
         RootCauseKind::SLAVE_INTERNAL_ERROR,
         {slaveFailure()}},
        {"port counters keep increasing",
         RootCauseKind::LINK_QUALITY_DEGRADATION,
         {qualityFailure()}}};

    const FaultMatrixSummary summary =
        RootCauseCalibration{}.evaluate(cases);
    std::cout << formatFaultMatrixSummary(summary) << '\n';
    return summary.passed == summary.entries.size() ? 0 : 1;
}
