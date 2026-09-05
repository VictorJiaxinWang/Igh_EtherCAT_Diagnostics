#include "ethercat_diag/root_cause/root_cause_formatter.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

namespace
{

RootCauseReport sampleReport()
{
    RootCauseReport report;
    report.timestamp_ms = 1234U;
    report.boundary = BoundaryEvidence{1, 2};
    report.conclusive = true;
    report.confidence = 0.75;
    report.primary.kind = RootCauseKind::BOUNDARY_LINK_FAILURE;
    report.primary.score = 90;
    report.primary.supporting_evidence = {
        "Slave1 Port0 lost-link counter increased by 1"};
    report.primary.contradicting_evidence = {
        "AL status was not available"};
    report.primary.recommended_actions = {
        "Inspect cable between Slave1 and Slave2"};
    report.candidates = {
        report.primary,
        {RootCauseKind::SLAVE_INTERNAL_ERROR, 25, {}, {}, {}}};
    report.collection_errors = {"AL Status: read failed \"once\""};
    return report;
}

void testHumanReadableReportContainsDecisionDetails()
{
    const std::string text =
        formatRootCauseReport(sampleReport());

    assert(text.find("[ROOT_CAUSE]") != std::string::npos);
    assert(text.find("BOUNDARY_LINK_FAILURE") != std::string::npos);
    assert(text.find("confidence=0.75") != std::string::npos);
    assert(text.find("Slave1<->Slave2") != std::string::npos);
    assert(text.find("supporting evidence") != std::string::npos);
    assert(text.find("recommended actions") != std::string::npos);
    assert(text.find("candidate scores") != std::string::npos);
    assert(text.find("SLAVE_INTERNAL_ERROR=25") != std::string::npos);
}

void testJsonLineIsStructuredAndEscaped()
{
    const std::string json =
        rootCauseReportToJson(sampleReport());

    assert(json.front() == '{');
    assert(json.back() == '}');
    assert(json.find("\"record\":\"root_cause\"") !=
           std::string::npos);
    assert(json.find("\"kind\":\"BOUNDARY_LINK_FAILURE\"") !=
           std::string::npos);
    assert(json.find("\"confidence\":0.750") !=
           std::string::npos);
    assert(json.find("read failed \\\"once\\\"") !=
           std::string::npos);
    assert(json.find("\"candidates\":[") != std::string::npos);
    assert(json.find(
        "{\"kind\":\"SLAVE_INTERNAL_ERROR\",\"score\":25}") !=
        std::string::npos);
}

void testAppendWritesExactlyOneJsonlRecord()
{
    const std::string path =
        "/tmp/ethercat_diag_v3_root_cause.jsonl";
    std::remove(path.c_str());

    assert(appendRootCauseReportJsonl(path, sampleReport()));

    std::ifstream input(path);
    const std::string contents(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());

    assert(contents == rootCauseReportToJson(sampleReport()) + "\n");
    std::remove(path.c_str());
}

void testAppendReportsOpenFailure()
{
    assert(!appendRootCauseReportJsonl(
        "/directory/that/does/not/exist/report.jsonl",
        sampleReport()));
}

} // namespace

int main()
{
    testHumanReadableReportContainsDecisionDetails();
    testJsonLineIsStructuredAndEscaped();
    testAppendWritesExactlyOneJsonlRecord();
    testAppendReportsOpenFailure();
    return 0;
}
