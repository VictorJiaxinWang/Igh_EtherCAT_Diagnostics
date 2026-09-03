#include "ethercat_diag/esc/diagnostic_formatter.h"

#include <sstream>

namespace
{

const char* yesNo(bool value)
{
    return value ? "yes" : "no";
}

} // namespace

std::string formatDlStatus(const DlStatusInfo& status)
{
    std::ostringstream output;

    output
        << "DL decoded: pdi="
        << (status.pdi_operational ? "operational" : "not operational")
        << ", watchdog="
        << (status.pdi_watchdog_reloaded ? "reloaded" : "expired")
        << ", enhanced_link="
        << yesNo(status.enhanced_link_detection);

    for (std::size_t port = 0; port < status.ports.size(); ++port)
    {
        const DlPortStatus& port_status = status.ports[port];

        output
            << '\n'
            << "Port " << port
            << ": link=" << yesNo(port_status.physical_link)
            << ", loop=" << (port_status.loop_closed ? "closed" : "open")
            << ", communication="
            << yesNo(port_status.communication_established);
    }

    return output.str();
}

std::string formatAlStatus(const AlStatusInfo& status)
{
    std::ostringstream output;

    output
        << "AL decoded: state=" << alStateName(status.state)
        << ", error=" << yesNo(status.error_indication)
        << ", warning=" << yesNo(status.warning_indication)
        << ", explicit_device_id_loaded="
        << yesNo(status.explicit_device_id_loaded);

    return output.str();
}

std::string formatAlStatusCode(
    const AlStatusCodeInfo& code,
    const std::optional<AlStatusInfo>& al_status
)
{
    std::ostringstream output;
    output << "AL code decoded: " << code.description;

    if (!al_status)
    {
        output << " [activity unknown: AL Status unavailable]";
        return output.str();
    }

    const AlStatusCodeContext context = classifyAlStatusCode(
        code.raw,
        al_status->error_indication,
        al_status->warning_indication
    );

    switch (context)
    {
    case AlStatusCodeContext::NoError:
        break;
    case AlStatusCodeContext::ActiveError:
        output << " [active error]";
        break;
    case AlStatusCodeContext::ActiveWarning:
        output << " [active warning]";
        break;
    case AlStatusCodeContext::Inactive:
        output << " [inactive/stale]";
        break;
    }

    return output.str();
}
