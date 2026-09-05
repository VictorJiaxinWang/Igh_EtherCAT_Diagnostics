#include "ethercat_diag/web/web_options.h"

#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

namespace
{

void testDefaults()
{
    const WebOptionsParseResult result = parseWebOptions({});

    assert(result.success);
    assert(!result.show_help);
    assert(result.options.bind_address == "0.0.0.0");
    assert(result.options.port == 8080);
    assert(result.options.data_directory == std::filesystem::path("logs"));
    assert(result.options.assets_directory == std::filesystem::path("web"));
}

void testOverrides()
{
    const WebOptionsParseResult result = parseWebOptions({
        "--bind", "127.0.0.1",
        "--port", "18080",
        "--data-dir", "/tmp/diag-data",
        "--assets-dir", "/tmp/diag-assets"});

    assert(result.success);
    assert(result.options.bind_address == "127.0.0.1");
    assert(result.options.port == 18080);
    assert(result.options.data_directory == "/tmp/diag-data");
    assert(result.options.assets_directory == "/tmp/diag-assets");
}

void testHelp()
{
    const WebOptionsParseResult result = parseWebOptions({"--help"});

    assert(result.success);
    assert(result.show_help);
    assert(webOptionsUsage().find("--data-dir") != std::string::npos);
}

void testRejectsInvalidArguments()
{
    const std::vector<std::vector<std::string>> cases{
        {"--port", "0"},
        {"--port", "65536"},
        {"--port", "abc"},
        {"--port"},
        {"--bind"},
        {"--unknown"}};

    for (const auto& arguments : cases)
    {
        const WebOptionsParseResult result = parseWebOptions(arguments);
        assert(!result.success);
        assert(!result.error.empty());
    }
}

}

int main()
{
    testDefaults();
    testOverrides();
    testHelp();
    testRejectsInvalidArguments();
    return 0;
}
