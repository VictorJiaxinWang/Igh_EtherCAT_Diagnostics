#include "ethercat_diag/web/http_router.h"
#include "ethercat_diag/web/http_server.h"
#include "ethercat_diag/web/web_options.h"

#include <csignal>
#include <iostream>
#include <string>
#include <vector>

namespace
{

volatile std::sig_atomic_t stop_requested = 0;

void handleStopSignal(int)
{
    stop_requested = 1;
}

}

int main(int argc, char* argv[])
{
    std::vector<std::string> arguments;
    for (int index = 1; index < argc; ++index)
    {
        arguments.emplace_back(argv[index]);
    }

    const WebOptionsParseResult parsed = parseWebOptions(arguments);
    if (!parsed.success)
    {
        std::cerr << "Error: " << parsed.error << "\n\n"
                  << webOptionsUsage();
        return 2;
    }
    if (parsed.show_help)
    {
        std::cout << webOptionsUsage();
        return 0;
    }

    if (std::signal(SIGINT, handleStopSignal) == SIG_ERR ||
        std::signal(SIGTERM, handleStopSignal) == SIG_ERR)
    {
        std::cerr << "Failed to install signal handlers\n";
        return 1;
    }

    HttpRouter router(
        parsed.options.data_directory,
        parsed.options.assets_directory);
    HttpServer server(
        router,
        parsed.options.bind_address,
        parsed.options.port);

    std::cout << "EtherCAT diagnostics Web UI: http://"
              << parsed.options.bind_address << ':'
              << parsed.options.port << '\n';

    const int result = server.run([]() { return stop_requested != 0; });
    if (result != 0)
    {
        std::cerr << "Web server failed: " << server.lastError() << '\n';
    }
    return result;
}
