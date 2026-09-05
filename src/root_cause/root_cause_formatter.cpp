#include "ethercat_diag/root_cause/root_cause_formatter.h"

#include <fstream>
#include <iomanip>
#include <sstream>

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

void appendJsonArray(
    std::ostringstream& output,
    const std::vector<std::string>& values)
{
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index != 0U)
        {
            output << ',';
        }
        output << '"' << escapeJson(values[index]) << '"';
    }
    output << ']';
}

std::string boundaryText(
    const std::optional<BoundaryEvidence>& boundary)
{
    if (!boundary)
    {
        return "unavailable";
    }
    if (boundary->last_alive_slave < 0)
    {
        return "Master<->Slave" +
            std::to_string(boundary->first_lost_slave);
    }
    return "Slave" + std::to_string(boundary->last_alive_slave) +
        "<->Slave" + std::to_string(boundary->first_lost_slave);
}

void appendList(
    std::ostringstream& output,
    const char* title,
    const std::vector<std::string>& values)
{
    if (values.empty())
    {
        return;
    }
    output << '\n' << title << ':';
    for (const std::string& value : values)
    {
        output << "\n  - " << value;
    }
}

} // namespace

const char* rootCauseKindToString(RootCauseKind kind) noexcept
{
    switch (kind)
    {
    case RootCauseKind::MASTER_LINK_FAILURE:
        return "MASTER_LINK_FAILURE";
    case RootCauseKind::BOUNDARY_LINK_FAILURE:
        return "BOUNDARY_LINK_FAILURE";
    case RootCauseKind::SLAVE_INTERNAL_ERROR:
        return "SLAVE_INTERNAL_ERROR";
    case RootCauseKind::LINK_QUALITY_DEGRADATION:
        return "LINK_QUALITY_DEGRADATION";
    case RootCauseKind::UNKNOWN:
        return "UNKNOWN";
    }
    return "UNKNOWN";
}

std::string formatRootCauseReport(const RootCauseReport& report)
{
    std::ostringstream output;
    output << "[ROOT_CAUSE] timestamp=" << report.timestamp_ms
           << " kind=" << rootCauseKindToString(report.primary.kind)
           << " score=" << report.primary.score
           << " confidence=" << std::fixed << std::setprecision(2)
           << report.confidence
           << " conclusive=" << (report.conclusive ? "yes" : "no")
           << " boundary=" << boundaryText(report.boundary);
    appendList(
        output,
        "supporting evidence",
        report.primary.supporting_evidence);
    appendList(
        output,
        "contradicting evidence",
        report.primary.contradicting_evidence);
    appendList(
        output,
        "recommended actions",
        report.primary.recommended_actions);
    if (!report.candidates.empty())
    {
        output << "\ncandidate scores:";
        for (const RootCauseCandidate& value : report.candidates)
        {
            output << "\n  - "
                   << rootCauseKindToString(value.kind)
                   << '=' << value.score;
        }
    }
    appendList(output, "collection errors", report.collection_errors);
    return output.str();
}

std::string rootCauseReportToJson(const RootCauseReport& report)
{
    std::ostringstream output;
    output << "{\"record\":\"root_cause\""
           << ",\"timestamp_ms\":" << report.timestamp_ms
           << ",\"kind\":\""
           << rootCauseKindToString(report.primary.kind) << '"'
           << ",\"score\":" << report.primary.score
           << ",\"confidence\":" << std::fixed
           << std::setprecision(3) << report.confidence
           << ",\"conclusive\":"
           << (report.conclusive ? "true" : "false");
    if (report.boundary)
    {
        output << ",\"boundary\":{\"last_alive_slave\":"
               << report.boundary->last_alive_slave
               << ",\"first_lost_slave\":"
               << report.boundary->first_lost_slave << '}';
    }
    else
    {
        output << ",\"boundary\":null";
    }
    output << ",\"supporting_evidence\":";
    appendJsonArray(output, report.primary.supporting_evidence);
    output << ",\"contradicting_evidence\":";
    appendJsonArray(output, report.primary.contradicting_evidence);
    output << ",\"recommended_actions\":";
    appendJsonArray(output, report.primary.recommended_actions);
    output << ",\"candidates\":[";
    for (std::size_t index = 0;
         index < report.candidates.size();
         ++index)
    {
        if (index != 0U)
        {
            output << ',';
        }
        output << "{\"kind\":\""
               << rootCauseKindToString(report.candidates[index].kind)
               << "\",\"score\":"
               << report.candidates[index].score
               << '}';
    }
    output << ']';
    output << ",\"collection_errors\":";
    appendJsonArray(output, report.collection_errors);
    output << '}';
    return output.str();
}

bool appendRootCauseReportJsonl(
    const std::string& file_path,
    const RootCauseReport& report)
{
    std::ofstream output(file_path, std::ios::app);
    if (!output.is_open())
    {
        return false;
    }
    output << rootCauseReportToJson(report) << '\n';
    return output.good();
}
