#include "ethercat_diag/publishing/web_data_publisher.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace
{

NetworkSnapshot makeSnapshot(bool link_up = true)
{
    NetworkSnapshot snapshot;
    snapshot.master.timestamp_ms = 1234U;
    snapshot.master.master_index = 0;
    snapshot.master.phase = "Idle";
    snapshot.master.active = false;
    snapshot.master.link_up = link_up;
    snapshot.master.slave_count = 2;
    snapshot.slaves = {
        {0, 1000, 0, AlState::PREOP, false, true, "Drive \"A\""},
        {1, 1000, 1, AlState::OP, false, true, "Drive B"}};
    return snapshot;
}

RootCauseReport makeRootCause()
{
    RootCauseReport report;
    report.timestamp_ms = 1234U;
    report.primary.kind = RootCauseKind::BOUNDARY_LINK_FAILURE;
    report.primary.score = 90;
    report.confidence = 0.75;
    report.conclusive = true;
    report.boundary = BoundaryEvidence{0, 1};
    return report;
}

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    return std::string(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
}

std::size_t countLines(const std::string& text)
{
    std::size_t count = 0U;
    for (char character : text)
    {
        if (character == '\n')
        {
            ++count;
        }
    }
    return count;
}

void testHealthyStatusJsonContainsMasterSlavesAndEscaping()
{
    const std::string json = makeLatestStatusJson(
        makeSnapshot(),
        std::nullopt,
        std::nullopt);

    assert(json.find("\"status\":\"HEALTHY\"") !=
           std::string::npos);
    assert(json.find("\"master_index\":0") != std::string::npos);
    assert(json.find("\"slave_count\":2") != std::string::npos);
    assert(json.find("Drive \\\"A\\\"") != std::string::npos);
    assert(json.find("\"last_fault_timestamp_ms\":null") !=
           std::string::npos);
    assert(json.back() == '}');
}

void testRootCauseMakesStatusFaultAndIsEmbedded()
{
    const std::string json = makeLatestStatusJson(
        makeSnapshot(),
        1200U,
        makeRootCause());

    assert(json.find("\"status\":\"FAULT\"") !=
           std::string::npos);
    assert(json.find("\"last_fault_timestamp_ms\":1200") !=
           std::string::npos);
    assert(json.find("\"kind\":\"BOUNDARY_LINK_FAILURE\"") !=
           std::string::npos);
}

void testInconclusiveQualityReportMakesStatusDegraded()
{
    RootCauseReport report;
    report.primary.kind = RootCauseKind::LINK_QUALITY_DEGRADATION;
    report.primary.score = 25;
    report.confidence = 1.0;
    report.conclusive = false;

    const std::string json = makeLatestStatusJson(
        makeSnapshot(),
        1200U,
        report);

    assert(json.find("\"status\":\"DEGRADED\"") !=
           std::string::npos);
}

void testAtomicPublishReplacesStatusAndLeavesNoTemporaryFile()
{
    const std::filesystem::path directory =
        "/tmp/ethercat_diag_v3_web_data";
    std::filesystem::remove_all(directory);

    WebDataPublisher publisher(directory);
    std::string error;
    assert(publisher.publishStatus(
        makeSnapshot(true), std::nullopt, std::nullopt, error));
    assert(error.empty());

    NetworkSnapshot changed = makeSnapshot(false);
    changed.master.timestamp_ms = 2000U;
    assert(publisher.publishStatus(
        changed, 2000U, std::nullopt, error));

    const std::string contents = readFile(publisher.statusPath());
    assert(contents.find("\"updated_ms\":2000") !=
           std::string::npos);
    assert(contents.find("\"link_up\":false") !=
           std::string::npos);
    assert(!std::filesystem::exists(
        publisher.statusPath().string() + ".tmp"));
    std::filesystem::remove_all(directory);
}

void testEventsJsonlAppendsFaultRecoveryAndRootCause()
{
    const std::filesystem::path directory =
        "/tmp/ethercat_diag_v3_event_data";
    std::filesystem::remove_all(directory);
    WebDataPublisher publisher(directory);
    std::string error;

    FaultEvent fault{
        1000U,
        EventType::SLAVE_LOST,
        1,
        2,
        1,
        "Slave \"1\" lost",
        -1};
    RecoveryEvent recovery;
    recovery.timestamp_ms = 2000U;
    recovery.fault_started_ms = 1000U;
    recovery.duration_ms = 1000U;
    recovery.fault_slave_count = 1;
    recovery.recovered_slave_count = 2;
    recovery.recovered_slave_positions = {1};
    recovery.description = "network recovered";

    assert(publisher.appendFaultEvents({fault}, error));
    assert(publisher.appendRecovery(recovery, error));
    assert(publisher.appendRootCause(makeRootCause(), error));

    const std::string contents = readFile(publisher.eventsPath());
    assert(countLines(contents) == 3U);
    assert(contents.find("\"record\":\"event\"") !=
           std::string::npos);
    assert(contents.find("\"record\":\"recovery\"") !=
           std::string::npos);
    assert(contents.find("\"record\":\"root_cause\"") !=
           std::string::npos);
    assert(contents.find("Slave \\\"1\\\" lost") !=
           std::string::npos);
    std::filesystem::remove_all(directory);
}

void testWriteFailureReturnsUsefulError()
{
    WebDataPublisher publisher("/proc/ethercat_diag_not_writable");
    std::string error;
    assert(!publisher.publishStatus(
        makeSnapshot(), std::nullopt, std::nullopt, error));
    assert(!error.empty());
}

} // namespace

int main()
{
    testHealthyStatusJsonContainsMasterSlavesAndEscaping();
    testRootCauseMakesStatusFaultAndIsEmbedded();
    testInconclusiveQualityReportMakesStatusDegraded();
    testAtomicPublishReplacesStatusAndLeavesNoTemporaryFile();
    testEventsJsonlAppendsFaultRecoveryAndRootCause();
    testWriteFailureReturnsUsefulError();
    return 0;
}
