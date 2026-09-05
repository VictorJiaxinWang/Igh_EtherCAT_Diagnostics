#include "ethercat_diag/web/web_data_store.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace
{

class TemporaryDirectory
{
public:
    TemporaryDirectory()
        : path_(
              std::filesystem::temp_directory_path() /
              ("ethercat-web-store-" + std::to_string(::getpid())))
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path& path() const
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void writeFile(
    const std::filesystem::path& path,
    const std::string& contents)
{
    std::ofstream output(path);
    output << contents;
}

void testStatusFileContract()
{
    TemporaryDirectory directory;
    WebDataStore store(directory.path());

    const WebDataResult missing = store.readStatus();
    assert(!missing.success);
    assert(missing.error == "diagnostic status is unavailable");

    writeFile(
        directory.path() / "latest_status.json",
        "{\"status\":\"HEALTHY\"}\n");

    const WebDataResult available = store.readStatus();
    assert(available.success);
    assert(available.body == "{\"status\":\"HEALTHY\"}\n");

    writeFile(directory.path() / "latest_status.json", "");
    assert(!store.readStatus().success);
}

void testMissingEventsAreAnEmptyHistory()
{
    TemporaryDirectory directory;
    WebDataStore store(directory.path());

    const WebDataResult result = store.readRecentEvents(50U);
    assert(result.success);
    assert(result.body == "[]\n");
}

void testReturnsOnlyRecentCompleteEventLines()
{
    TemporaryDirectory directory;
    writeFile(
        directory.path() / "events.jsonl",
        "{\"id\":1}\n"
        "\n"
        "{\"id\":2}\n"
        "{\"id\":3}\n"
        "{\"id\":4");
    WebDataStore store(directory.path());

    const WebDataResult result = store.readRecentEvents(2U);

    assert(result.success);
    assert(result.body == "[{\"id\":2},{\"id\":3}]\n");
}

}

int main()
{
    testStatusFileContract();
    testMissingEventsAreAnEmptyHistory();
    testReturnsOnlyRecentCompleteEventLines();
    return 0;
}
