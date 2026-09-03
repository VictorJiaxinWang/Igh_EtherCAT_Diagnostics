#include "ethercat_diag/esc/esc_diagnostic_reader.h"
#include "ethercat_diag/cli/probe_options.h"
#include "ethercat_diag/esc/al_status_decoder.h"
#include "ethercat_diag/esc/dl_status_decoder.h"
#include "ethercat_diag/esc/al_status_code_decoder.h"
#include "ethercat_diag/esc/diagnostic_formatter.h"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <optional>

namespace
{

void printRegisterResult(
    const std::string& name,
    std::uint16_t address,
    const RegisterReadResult& result
)
{
    std::cout
        << name
        << " [0x"
        << std::hex
        << std::setfill('0')
        << std::setw(4)
        << address
        << "]: ";

    if (result.success)
    {
        std::cout
            << "0x"
            << std::setw(4)
            << result.value;
    }
    else
    {
        std::cout
            << "ERROR: "
            << result.error;
    }

    std::cout << std::dec << '\n';
}

} // namespace

int main(int argc, char* argv[])
{
    std::vector<std::string> arguments;

    for (int index = 1; index < argc; ++index)
    {
        arguments.emplace_back(argv[index]);
    }

    const std::optional<ProbeOptions> options =
        parseProbeOptions(arguments);

    if (!options)
    {
        std::cerr
            << "Usage: "
            << argv[0]
            << " <master_index> <slave_position>\n";

        return 2;
    }

    EscDiagnosticReader reader;

    const EscDiagnosticSample sample =
        reader.read(
            options->master_index,
            options->slave_position
        );

    std::cout
        << "ESC diagnostic sample: master "
        << sample.master_index
        << ", slave "
        << sample.slave_position
        << '\n';

    printRegisterResult(
        "DL Status",
        0x0110,
        sample.dl_status
    );

    if (sample.dl_status.success)
    {
        const DlStatusInfo dl_decoded =
            decodeDlStatus(sample.dl_status.value);

        std::cout << formatDlStatus(dl_decoded) << '\n';
    }

    printRegisterResult(
        "AL Status",
        0x0130,
        sample.al_status
    );

    std::optional<AlStatusInfo> decoded_al_status;

    if (sample.al_status.success)
    {
        decoded_al_status = decodeAlStatus(
            sample.al_status.value
        );

        std::cout << formatAlStatus(*decoded_al_status) << '\n';
    }

    printRegisterResult(
        "AL Status Code",
        0x0134,
        sample.al_status_code
    );

    if (sample.al_status_code.success)
    {
        const AlStatusCodeInfo al_code_decoded =
            decodeAlStatusCode(sample.al_status_code.value);

        std::cout
            << formatAlStatusCode(al_code_decoded, decoded_al_status)
            << '\n';
    }

    return sample.complete() ? 0 : 1;
}
