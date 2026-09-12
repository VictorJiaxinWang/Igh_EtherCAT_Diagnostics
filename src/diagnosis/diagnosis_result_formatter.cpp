#include "ethercat_diag/diagnosis/diagnosis_result_formatter.h"

#include "ethercat_diag/esc/al_status_code_decoder.h"
#include "ethercat_diag/esc/al_status_decoder.h"
#include "ethercat_diag/esc/diagnostic_formatter.h"
#include "ethercat_diag/esc/dl_status_decoder.h"
#include "ethercat_diag/esc/port_error_formatter.h"

#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

namespace
{

std::string formatHex16(std::uint16_t value)
{
    std::ostringstream output;

    output
        << "0x"
        << std::hex
        << std::setw(4)
        << std::setfill('0')
        << value;
    return output.str();
}

std::string formatBoundary(const FaultBoundary& boundary)
{
    if (!boundary.valid ||
        boundary.first_lost_slave < 0)
    {
        return "unavailable";
    }

    std::ostringstream output;

    if (boundary.first_lost_alias > 0)
    {
        if (boundary.last_alive_alias > 0)
        {
            output << "Alias" << boundary.last_alive_alias << ':'
                   << boundary.last_alive_relative_position;
        }
        else
        {
            output << "Master";
        }
        output << "<->Alias" << boundary.first_lost_alias << ':'
               << boundary.first_lost_relative_position;
        return output.str();
    }

    if (boundary.last_alive_slave == -1 &&
        boundary.first_lost_slave == 0)
    {
        output << "Master";
    }
    else if (boundary.last_alive_slave >= 0 &&
             boundary.last_alive_slave <
                 boundary.first_lost_slave)
    {
        output << "Slave" << boundary.last_alive_slave;
    }
    else
    {
        return "unavailable";
    }

    output
        << "<->Slave"
        << boundary.first_lost_slave;
    return output.str();
}

void appendDlStatus(
    std::ostringstream& output,
    const RegisterReadResult& read_result)
{
    output << '\n' << "DL Status [0x0110]: ";

    if (!read_result.success)
    {
        output << "ERROR: " << read_result.error;
        return;
    }

    output << formatHex16(read_result.value);
    output
        << '\n'
        << formatDlStatus(
               decodeDlStatus(read_result.value));
}

std::optional<AlStatusInfo> appendAlStatus(
    std::ostringstream& output,
    const RegisterReadResult& read_result)
{
    output << '\n' << "AL Status [0x0130]: ";

    if (!read_result.success)
    {
        output << "ERROR: " << read_result.error;
        return std::nullopt;
    }

    const AlStatusInfo status =
        decodeAlStatus(read_result.value);

    output << formatHex16(read_result.value);
    output << '\n' << formatAlStatus(status);
    return status;
}

void appendAlStatusCode(
    std::ostringstream& output,
    const RegisterReadResult& read_result,
    const std::optional<AlStatusInfo>& al_status)
{
    output << '\n' << "AL Status Code [0x0134]: ";

    if (!read_result.success)
    {
        output << "ERROR: " << read_result.error;
        return;
    }

    output << formatHex16(read_result.value);
    output
        << '\n'
        << formatAlStatusCode(
               decodeAlStatusCode(read_result.value),
               al_status);
}

} // namespace

std::string formatDiagnosisResult(
    const DiagResult& result)
{
    std::ostringstream output;

    const char* status = "rejected";
    if (result.attempted())
    {
        status = result.success()
            ? "success"
            : "partial";
    }

    output
        << "[ACTIVE_DIAG] master="
        << result.master_index
        << " boundary="
        << formatBoundary(result.boundary)
        << " status="
        << status;

    if (!result.attempted())
    {
        if (!result.error.empty())
        {
            output
                << '\n'
                << "error=\""
                << result.error
                << '"';
        }
        return output.str();
    }

    const EscDiagnosticSample& sample =
        *result.sample;

    appendDlStatus(output, sample.dl_status);

    const std::optional<AlStatusInfo> al_status =
        appendAlStatus(output, sample.al_status);

    appendAlStatusCode(
        output,
        sample.al_status_code,
        al_status);

    if (result.port_errors)
    {
        output
            << '\n'
            << "Port Error Counters [0x0300..0x0313]: ";

        if (!result.port_errors->success)
        {
            output
                << "ERROR: "
                << result.port_errors->error;
        }
        else
        {
            output
                << "raw cumulative values\n"
                << formatPortErrorCounters(
                       result.port_errors->counters);
        }
    }

    return output.str();
}
