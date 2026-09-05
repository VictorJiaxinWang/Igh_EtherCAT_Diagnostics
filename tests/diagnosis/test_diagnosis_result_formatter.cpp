#include "ethercat_diag/diagnosis/diagnosis_result_formatter.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace
{

void requireContains(
    const std::string& text,
    std::string_view expected,
    std::string_view message)
{
    if (text.find(expected) != std::string::npos)
    {
        return;
    }

    std::cerr
        << "FAILED: " << message << '\n'
        << "Expected to contain: " << expected << '\n'
        << "Actual report:\n" << text << '\n';
    std::exit(EXIT_FAILURE);
}

EscDiagnosticSample makeCompleteSample()
{
    EscDiagnosticSample sample;
    sample.master_index = 0;
    sample.slave_position = 2;
    sample.dl_status = {true, 0x5613, {}};
    sample.al_status = {true, 0x0002, {}};
    sample.al_status_code = {true, 0x0000, {}};
    return sample;
}

void testFormatsCompleteDiagnosis()
{
    DiagResult result;
    result.master_index = 0;
    result.boundary = {2, 3, true};
    result.sample = makeCompleteSample();

    const std::string report =
        formatDiagnosisResult(result);

    requireContains(
        report,
        "[ACTIVE_DIAG] master=0 boundary=Slave2<->Slave3 status=success",
        "complete diagnosis has success summary");
    requireContains(
        report,
        "DL Status [0x0110]: 0x5613",
        "complete diagnosis includes raw DL status");
    requireContains(
        report,
        "DL decoded: pdi=operational",
        "complete diagnosis decodes DL status");
    requireContains(
        report,
        "AL Status [0x0130]: 0x0002",
        "complete diagnosis includes raw AL status");
    requireContains(
        report,
        "AL decoded: state=PREOP",
        "complete diagnosis decodes AL status");
    requireContains(
        report,
        "AL Status Code [0x0134]: 0x0000",
        "complete diagnosis includes raw AL status code");
    requireContains(
        report,
        "AL code decoded: No error",
        "complete diagnosis decodes AL status code");
}

void testFormatsRejectedDiagnosis()
{
    DiagResult result;
    result.master_index = 0;
    result.error = "Invalid fault boundary";

    const std::string report =
        formatDiagnosisResult(result);

    requireContains(
        report,
        "[ACTIVE_DIAG] master=0 boundary=unavailable status=rejected",
        "rejected diagnosis has rejection summary");
    requireContains(
        report,
        "error=\"Invalid fault boundary\"",
        "rejected diagnosis preserves its error");
}

void testFormatsPartialDiagnosisWithoutDiscardingGoodData()
{
    DiagResult result;
    result.master_index = 0;
    result.boundary = {2, 3, true};

    EscDiagnosticSample sample;
    sample.master_index = 0;
    sample.slave_position = 2;
    sample.dl_status = {true, 0x5613, {}};
    sample.al_status = {false, 0, "AL read failed"};
    sample.al_status_code = {true, 0x0000, {}};
    result.sample = sample;

    const std::string report =
        formatDiagnosisResult(result);

    requireContains(
        report,
        "status=partial",
        "incomplete sample is reported as partial");
    requireContains(
        report,
        "DL Status [0x0110]: 0x5613",
        "partial diagnosis preserves successful DL data");
    requireContains(
        report,
        "AL Status [0x0130]: ERROR: AL read failed",
        "partial diagnosis reports failed AL read");
    requireContains(
        report,
        "AL Status Code [0x0134]: 0x0000",
        "partial diagnosis preserves successful AL code data");
    requireContains(
        report,
        "activity unknown: AL Status unavailable",
        "AL code context handles missing AL status");
}

void testFormatsMasterToFirstSlaveBoundary()
{
    DiagResult result;
    result.master_index = 0;
    result.boundary = {-1, 0, true};
    result.error = "Negative slave position";

    const std::string report =
        formatDiagnosisResult(result);

    requireContains(
        report,
        "boundary=Master<->Slave0 status=rejected",
        "first slave loss names the master-side boundary");
}

void testFormatsPortErrorEvidence()
{
    DiagResult result;
    result.master_index = 0;
    result.boundary = {2, 3, true};
    result.sample = makeCompleteSample();

    PortErrorCounters counters;
    counters.ports[0] = {1U, 2U, 3U, 4U};
    result.port_errors = PortErrorReadResult{true, counters, {}};

    const std::string report = formatDiagnosisResult(result);

    requireContains(
        report,
        "Port Error Counters [0x0300..0x0313]",
        "active diagnosis labels port counter evidence");
    requireContains(
        report,
        "Port 0: invalid_frame=1, rx_error=2, "
        "forwarded_rx_error=3, lost_link=4",
        "active diagnosis formats decoded port counters");
}

void testInvalidBoundaryCoordinatesAreUnavailable()
{
    const FaultBoundary invalid_boundaries[] = {
        {3, -1, true},
        {-2, 0, true},
        {3, 3, true}
    };

    for (const FaultBoundary& boundary : invalid_boundaries)
    {
        DiagResult result;
        result.master_index = 0;
        result.boundary = boundary;
        result.error = "Invalid slave position";

        requireContains(
            formatDiagnosisResult(result),
            "boundary=unavailable status=rejected",
            "invalid boundary coordinates are not presented as valid");
    }
}

} // namespace

int main()
{
    testFormatsCompleteDiagnosis();
    testFormatsRejectedDiagnosis();
    testFormatsPartialDiagnosisWithoutDiscardingGoodData();
    testFormatsMasterToFirstSlaveBoundary();
    testFormatsPortErrorEvidence();
    testInvalidBoundaryCoordinatesAreUnavailable();

    std::cout
        << "All diagnosis result formatter tests passed\n";
    return 0;
}
