#include "ethercat_diag/recording/blackbox_filename.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>

void testAddsSuffixWhenFileAlreadyExists()
{
    const std::filesystem::path test_directory =
        std::filesystem::temp_directory_path() /
        "ethercat_diag_filename_test";

    std::filesystem::remove_all(test_directory);
    std::filesystem::create_directories(
        test_directory);

    const std::uint64_t timestamp_ms = 123456;

    const std::filesystem::path first_path =
        makeUniqueBlackboxPath(
            test_directory,
            timestamp_ms);

    assert(
        first_path.filename() ==
        "fault_123456.jsonl");

    std::ofstream(first_path).close();

    const std::filesystem::path second_path =
        makeUniqueBlackboxPath(
            test_directory,
            timestamp_ms);

    assert(
        second_path.filename() ==
        "fault_123456_1.jsonl");

    std::ofstream(second_path).close();

    const std::filesystem::path third_path =
        makeUniqueBlackboxPath(
            test_directory,
            timestamp_ms);

    assert(
        third_path.filename() ==
        "fault_123456_2.jsonl");

    std::filesystem::remove_all(test_directory);
}

int main()
{
    testAddsSuffixWhenFileAlreadyExists();

    std::cout
        << "All Blackbox filename tests passed\n";

    return 0;
}
