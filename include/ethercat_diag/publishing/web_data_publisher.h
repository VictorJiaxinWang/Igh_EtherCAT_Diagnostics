#pragma once

#include "ethercat_diag/common/diag_types.h"
#include "ethercat_diag/root_cause/root_cause_report.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

std::string makeLatestStatusJson(
    const NetworkSnapshot& snapshot,
    const std::optional<std::uint64_t>& last_fault_timestamp_ms,
    const std::optional<RootCauseReport>& root_cause);

class WebDataPublisher
{
public:
    explicit WebDataPublisher(std::filesystem::path directory);

    bool publishStatus(
        const NetworkSnapshot& snapshot,
        const std::optional<std::uint64_t>& last_fault_timestamp_ms,
        const std::optional<RootCauseReport>& root_cause,
        std::string& error) const;

    bool appendFaultEvents(
        const std::vector<FaultEvent>& events,
        std::string& error) const;

    bool appendRecovery(
        const RecoveryEvent& recovery,
        std::string& error) const;

    bool appendRootCause(
        const RootCauseReport& root_cause,
        std::string& error) const;

    std::filesystem::path statusPath() const;
    std::filesystem::path eventsPath() const;

private:
    bool ensureDirectory(std::string& error) const;
    bool appendLine(const std::string& line, std::string& error) const;

    std::filesystem::path directory_;
};
