#include "ethercat_diag/esc/diagnostic_formatter.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace
{

void expectEqual(
    std::string_view actual,
    std::string_view expected,
    std::string_view test_name
)
{
    if (actual == expected)
    {
        return;
    }

    std::cerr
        << "FAILED: " << test_name << '\n'
        << "expected: " << expected << '\n'
        << "actual  : " << actual << '\n';
    std::exit(EXIT_FAILURE);
}

void testFormatsDlStatus()
{
    DlStatusInfo status;
    status.pdi_operational = true;
    status.pdi_watchdog_reloaded = true;
    status.enhanced_link_detection = false;
    status.ports[0] = {true, false, true};
    status.ports[1] = {false, true, false};
    status.ports[2] = {false, true, false};
    status.ports[3] = {false, true, false};

    const std::string expected =
        "DL decoded: pdi=operational, watchdog=reloaded, enhanced_link=no\n"
        "Port 0: link=yes, loop=open, communication=yes\n"
        "Port 1: link=no, loop=closed, communication=no\n"
        "Port 2: link=no, loop=closed, communication=no\n"
        "Port 3: link=no, loop=closed, communication=no";

    expectEqual(formatDlStatus(status), expected, "formats DL status");
}

void testFormatsAlStatus()
{
    AlStatusInfo status;
    status.state = EscAlState::PreOp;

    expectEqual(
        formatAlStatus(status),
        "AL decoded: state=PREOP, error=no, warning=no, "
        "explicit_device_id_loaded=no",
        "formats AL status"
    );
}

void testFormatsNoErrorCode()
{
    const AlStatusCodeInfo code = decodeAlStatusCode(0x0000);
    const AlStatusInfo status{};

    expectEqual(
        formatAlStatusCode(code, status),
        "AL code decoded: No error",
        "formats no-error code"
    );
}

void testFormatsActiveErrorCode()
{
    const AlStatusCodeInfo code = decodeAlStatusCode(0x001B);
    AlStatusInfo status;
    status.error_indication = true;

    expectEqual(
        formatAlStatusCode(code, status),
        "AL code decoded: SyncManager watchdog [active error]",
        "formats active error code"
    );
}

void testFormatsActiveWarningCode()
{
    const AlStatusCodeInfo code = decodeAlStatusCode(0x0035);
    AlStatusInfo status;
    status.warning_indication = true;

    expectEqual(
        formatAlStatusCode(code, status),
        "AL code decoded: Invalid DC sync cycle time [active warning]",
        "formats active warning code"
    );
}

void testFormatsInactiveCode()
{
    const AlStatusCodeInfo code = decodeAlStatusCode(0x001B);
    const AlStatusInfo status{};

    expectEqual(
        formatAlStatusCode(code, status),
        "AL code decoded: SyncManager watchdog [inactive/stale]",
        "formats inactive code"
    );
}

void testFormatsCodeWhenAlStatusIsUnavailable()
{
    const AlStatusCodeInfo code = decodeAlStatusCode(0x001B);

    expectEqual(
        formatAlStatusCode(code, std::nullopt),
        "AL code decoded: SyncManager watchdog "
        "[activity unknown: AL Status unavailable]",
        "formats code without AL status"
    );
}

void testFormatsUnknownActiveErrorCode()
{
    const AlStatusCodeInfo code = decodeAlStatusCode(0xFFFF);
    AlStatusInfo status;
    status.error_indication = true;

    expectEqual(
        formatAlStatusCode(code, status),
        "AL code decoded: Unknown AL status code [active error]",
        "formats unknown active error code"
    );
}

void testZeroCodeTakesPriorityOverFlags()
{
    const AlStatusCodeInfo code = decodeAlStatusCode(0x0000);
    AlStatusInfo status;
    status.error_indication = true;
    status.warning_indication = true;

    expectEqual(
        formatAlStatusCode(code, status),
        "AL code decoded: No error",
        "zero code takes priority over flags"
    );
}

} // namespace

int main()
{
    testFormatsDlStatus();
    testFormatsAlStatus();
    testFormatsNoErrorCode();
    testFormatsActiveErrorCode();
    testFormatsActiveWarningCode();
    testFormatsInactiveCode();
    testFormatsCodeWhenAlStatusIsUnavailable();
    testFormatsUnknownActiveErrorCode();
    testZeroCodeTakesPriorityOverFlags();

    std::cout << "All diagnostic formatter tests passed\n";
    return 0;
}
