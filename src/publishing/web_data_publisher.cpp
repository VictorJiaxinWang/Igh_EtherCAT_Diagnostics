#include "ethercat_diag/publishing/web_data_publisher.h"

#include "ethercat_diag/root_cause/root_cause_formatter.h"

#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>
#include <algorithm>

namespace
{

std::string escapeJson(const std::string& input)
{
    static constexpr char hex_digits[] = "0123456789abcdef";
    std::string escaped;
    for (const unsigned char character : input)
    {
        switch (character)
        {
        case '"': escaped += "\\\""; break;
        case '\\': escaped += "\\\\"; break;
        case '\b': escaped += "\\b"; break;
        case '\f': escaped += "\\f"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (character < 0x20U)
            {
                escaped += "\\u00";
                escaped += hex_digits[(character >> 4U) & 0x0fU];
                escaped += hex_digits[character & 0x0fU];
            }
            else
            {
                escaped += static_cast<char>(character);
            }
        }
    }
    return escaped;
}

const char* alStateToString(AlState state)
{
    switch (state)
    {
    case AlState::INIT: return "INIT";
    case AlState::PREOP: return "PREOP";
    case AlState::SAFEOP: return "SAFEOP";
    case AlState::OP: return "OP";
    case AlState::UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN";
}

const char* eventTypeToString(EventType type)
{
    switch (type)
    {
    case EventType::MASTER_LINK_DOWN: return "MASTER_LINK_DOWN";
    case EventType::MASTER_LINK_UP: return "MASTER_LINK_UP";
    case EventType::SLAVE_COUNT_CHANGED: return "SLAVE_COUNT_CHANGED";
    case EventType::SLAVE_LOST: return "SLAVE_LOST";
    case EventType::SLAVE_STATE_CHANGED: return "SLAVE_STATE_CHANGED";
    case EventType::PORT_INVALID_FRAME_INCREASED:
        return "PORT_INVALID_FRAME_INCREASED";
    case EventType::PORT_RX_ERROR_INCREASED:
        return "PORT_RX_ERROR_INCREASED";
    case EventType::PORT_FORWARDED_RX_ERROR_INCREASED:
        return "PORT_FORWARDED_RX_ERROR_INCREASED";
    case EventType::PORT_LOST_LINK_INCREASED:
        return "PORT_LOST_LINK_INCREASED";
    }
    return "UNKNOWN_EVENT";
}

const char* healthStatus(
    const NetworkSnapshot& snapshot,
    const std::optional<RootCauseReport>& root_cause)
{
    if (!snapshot.master.link_up)
    {
        return "FAULT";
    }
    for (const SlaveSnapshot& slave : snapshot.slaves)
    {
        if (!slave.online || slave.has_error)
        {
            return "FAULT";
        }
    }
    if (!root_cause ||
        root_cause->primary.kind == RootCauseKind::UNKNOWN)
    {
        return "HEALTHY";
    }
    if (root_cause->primary.kind ==
            RootCauseKind::LINK_QUALITY_DEGRADATION ||
        !root_cause->conclusive)
    {
        return "DEGRADED";
    }
    return "FAULT";
}

std::string faultEventJson(const FaultEvent& event)
{
    std::ostringstream output;
    output << "{\"record\":\"event\""
           << ",\"timestamp_ms\":" << event.timestamp_ms
           << ",\"type\":\"" << eventTypeToString(event.type) << '"'
           << ",\"master_index\":" << event.master_index
           << ",\"slave_position\":" << event.slave_position
           << ",\"slave_alias\":" << event.slave_alias
           << ",\"slave_relative_position\":"
           << event.slave_relative_position
           << ",\"port_position\":" << event.port_position
           << ",\"old_value\":" << event.old_value
           << ",\"new_value\":" << event.new_value
           << ",\"description\":\""
           << escapeJson(event.description) << "\"}";
    return output.str();
}

std::string recoveryJson(const RecoveryEvent& recovery)
{
    std::ostringstream output;
    output << "{\"record\":\"recovery\""
           << ",\"timestamp_ms\":" << recovery.timestamp_ms
           << ",\"fault_started_ms\":" << recovery.fault_started_ms
           << ",\"duration_ms\":" << recovery.duration_ms
           << ",\"fault_slave_count\":" << recovery.fault_slave_count
           << ",\"recovered_slave_count\":"
           << recovery.recovered_slave_count
           << ",\"recovered_slave_positions\":[";
    for (std::size_t index = 0;
         index < recovery.recovered_slave_positions.size();
         ++index)
    {
        if (index != 0U)
        {
            output << ',';
        }
        output << recovery.recovered_slave_positions[index];
    }
    output << "],\"description\":\""
           << escapeJson(recovery.description) << "\"}";
    return output.str();
}

} // namespace

std::string makeLatestStatusJson(
    const NetworkSnapshot& snapshot,
    const std::optional<std::uint64_t>& last_fault_timestamp_ms,
    const std::optional<RootCauseReport>& root_cause)
{
    const MasterSnapshot& master = snapshot.master;
    std::ostringstream output;
    output << "{\"updated_ms\":" << master.timestamp_ms
           << ",\"status\":\"" << healthStatus(snapshot, root_cause) << '"'
           << ",\"master_index\":" << master.master_index
           << ",\"phase\":\"" << escapeJson(master.phase) << '"'
           << ",\"active\":" << (master.active ? "true" : "false")
           << ",\"link_up\":" << (master.link_up ? "true" : "false")
           << ",\"slave_count\":" << master.slave_count
           << ",\"last_fault_timestamp_ms\":";
    if (last_fault_timestamp_ms)
    {
        output << *last_fault_timestamp_ms;
    }
    else
    {
        output << "null";
    }
    output << ",\"slaves\":[";
    for (std::size_t index = 0; index < snapshot.slaves.size(); ++index)
    {
        if (index != 0U)
        {
            output << ',';
        }
        const SlaveSnapshot& slave = snapshot.slaves[index];
        output << "{\"position\":" << slave.position
               << ",\"alias\":" << slave.alias
               << ",\"relative_position\":" << slave.relative_position
               << ",\"state\":\"" << alStateToString(slave.state) << '"'
               << ",\"has_error\":"
               << (slave.has_error ? "true" : "false")
               << ",\"online\":" << (slave.online ? "true" : "false")
               << ",\"name\":\"" << escapeJson(slave.name) << "\"}";
    }
    output << "],\"root_cause\":";
    if (root_cause)
    {
        output << rootCauseReportToJson(*root_cause);
    }
    else
    {
        output << "null";
    }
    output << '}';
    return output.str();
}

std::string makeMultiMasterStatusJson(
    const std::vector<PublishedMasterStatus>& masters)
{
    std::uint64_t updated = 0U;
    int severity = 0;
    for (const PublishedMasterStatus& master : masters)
    {
        updated = std::max(updated, master.snapshot.master.timestamp_ms);
        const std::string status = healthStatus(master.snapshot, master.root_cause);
        severity = std::max(severity,
            status == "FAULT" ? 2 : (status == "DEGRADED" ? 1 : 0));
    }
    std::ostringstream output;
    output << "{\"schema_version\":2,\"updated_ms\":" << updated
           << ",\"status\":\""
           << (severity == 2 ? "FAULT" : (severity == 1 ? "DEGRADED" : "HEALTHY"))
           << "\",\"masters\":[";
    for (std::size_t index = 0; index < masters.size(); ++index)
    {
        if (index != 0U) output << ',';
        output << makeLatestStatusJson(
            masters[index].snapshot,
            masters[index].last_fault_timestamp_ms,
            masters[index].root_cause);
    }
    output << "]}";
    return output.str();
}

WebDataPublisher::WebDataPublisher(std::filesystem::path directory)
    : directory_(std::move(directory))
{
}

std::filesystem::path WebDataPublisher::statusPath() const
{
    return directory_ / "latest_status.json";
}

std::filesystem::path WebDataPublisher::eventsPath() const
{
    return directory_ / "events.jsonl";
}

bool WebDataPublisher::ensureDirectory(std::string& error) const
{
    std::error_code filesystem_error;
    std::filesystem::create_directories(directory_, filesystem_error);
    if (filesystem_error)
    {
        error = "failed to create data directory: " +
            filesystem_error.message();
        return false;
    }
    return true;
}

bool WebDataPublisher::publishStatus(
    const NetworkSnapshot& snapshot,
    const std::optional<std::uint64_t>& last_fault_timestamp_ms,
    const std::optional<RootCauseReport>& root_cause,
    std::string& error) const
{
    error.clear();
    if (!ensureDirectory(error))
    {
        return false;
    }

    const std::filesystem::path temporary =
        statusPath().string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output.is_open())
    {
        error = "failed to open temporary status file";
        return false;
    }
    output << makeLatestStatusJson(
                  snapshot,
                  last_fault_timestamp_ms,
                  root_cause)
           << '\n';
    output.close();
    if (!output)
    {
        error = "failed to write temporary status file";
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }

    std::error_code rename_error;
    std::filesystem::rename(temporary, statusPath(), rename_error);
    if (rename_error)
    {
        error = "failed to replace latest status: " +
            rename_error.message();
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    return true;
}

bool WebDataPublisher::publishStatus(
    const std::vector<PublishedMasterStatus>& masters,
    std::string& error) const
{
    error.clear();
    if (masters.empty() || !ensureDirectory(error))
    {
        if (error.empty()) error = "no master status to publish";
        return false;
    }
    const std::filesystem::path temporary = statusPath().string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output.is_open())
    {
        error = "failed to open temporary status file";
        return false;
    }
    output << makeMultiMasterStatusJson(masters) << '\n';
    output.close();
    if (!output)
    {
        error = "failed to write temporary status file";
        return false;
    }
    std::error_code rename_error;
    std::filesystem::rename(temporary, statusPath(), rename_error);
    if (rename_error)
    {
        error = "failed to replace latest status: " + rename_error.message();
        return false;
    }
    return true;
}

bool WebDataPublisher::appendLine(
    const std::string& line,
    std::string& error) const
{
    error.clear();
    if (!ensureDirectory(error))
    {
        return false;
    }
    std::ofstream output(eventsPath(), std::ios::app);
    if (!output.is_open())
    {
        error = "failed to open events JSONL";
        return false;
    }
    output << line << '\n';
    if (!output.good())
    {
        error = "failed to append events JSONL";
        return false;
    }
    return true;
}

bool WebDataPublisher::appendFaultEvents(
    const std::vector<FaultEvent>& events,
    std::string& error) const
{
    for (const FaultEvent& event : events)
    {
        if (!appendLine(faultEventJson(event), error))
        {
            return false;
        }
    }
    error.clear();
    return true;
}

bool WebDataPublisher::appendRecovery(
    const RecoveryEvent& recovery,
    std::string& error) const
{
    return appendLine(recoveryJson(recovery), error);
}

bool WebDataPublisher::appendRecovery(
    int master_index,
    const RecoveryEvent& recovery,
    std::string& error) const
{
    std::string json = recoveryJson(recovery);
    json.insert(1U, "\"master_index\":" + std::to_string(master_index) + ",");
    return appendLine(json, error);
}

bool WebDataPublisher::appendRootCause(
    const RootCauseReport& root_cause,
    std::string& error) const
{
    return appendLine(rootCauseReportToJson(root_cause), error);
}

bool WebDataPublisher::appendRootCause(
    int master_index,
    const RootCauseReport& root_cause,
    std::string& error) const
{
    std::string json = rootCauseReportToJson(root_cause);
    json.insert(1U, "\"master_index\":" + std::to_string(master_index) + ",");
    return appendLine(json, error);
}
