#include "ethercat_diag/esc/al_status_code_decoder.h"

#include <iostream>
#include <cassert>

struct CodeCase
{
    std::uint16_t raw;
    std::string_view expected_description;
};

void testAlStatusCodeNoError()
{
    const AlStatusCodeInfo info = decodeAlStatusCode(0x0000);

    assert(info.raw == 0x0000);
    assert(info.known);
    assert(info.description == "No error");
}

void testAlStatusCodeKnownError()
{
    const AlStatusCodeInfo info = decodeAlStatusCode(0x001B);

    assert(info.raw == 0x001B);
    assert(info.known);
    assert(info.description == "SyncManager watchdog");
}

void testAlStatusCodeUnknownError()
{
    const AlStatusCodeInfo info = decodeAlStatusCode(0xFFFF);

    assert(info.raw == 0xFFFF);
    assert(!info.known);
    assert(info.description == "Unknown AL status code");
}

void testAlStatusCodeFullCases()
{
    const CodeCase cases[]{
        {0x0000, "No error"},
        {0x0003, "Invalid device setup"},
        {0x0011, "Invalid requested state change"},
        {0x0016, "Invalid mailbox configuration"},
        {0x001A, "Synchronization error"},
        {0x001B, "SyncManager watchdog"},
        {0x001D, "Invalid output configuration"},
        {0x001E, "Invalid input configuration"},
        {0x002C, "Fatal sync error"},
        {0x002D, "No sync error"},
        {0x0035, "Invalid DC sync cycle time"},
        {0x0052, "External hardware not ready"}
    };

    for (const CodeCase& test_case: cases)
    {
        const AlStatusCodeInfo info = decodeAlStatusCode(test_case.raw);

        assert(info.raw == test_case.raw);
        assert(info.description == test_case.expected_description);
        assert(info.known == true);
    }
}

void testClassifyAlStatusCode()
{
    assert(classifyAlStatusCode(0x0000, false, false) == AlStatusCodeContext::NoError);
    assert(classifyAlStatusCode(0x001B, true, false) == AlStatusCodeContext::ActiveError);
    assert(classifyAlStatusCode(0x0035, false, true) == AlStatusCodeContext::ActiveWarning);
    assert(classifyAlStatusCode(0x001B, false, false) == AlStatusCodeContext::Inactive);
    assert(classifyAlStatusCode(0x001B, true, true) == AlStatusCodeContext::ActiveError);
    assert(classifyAlStatusCode(0x0000, true, true) == AlStatusCodeContext::NoError);
}

int main()
{
    testAlStatusCodeNoError();
    testAlStatusCodeKnownError();
    testAlStatusCodeUnknownError();
    testAlStatusCodeFullCases();
    testClassifyAlStatusCode();

    std::cout<<"All the test cases passed!\n";
    return 0;
}
