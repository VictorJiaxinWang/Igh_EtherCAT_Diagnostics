#include "ethercat_diag/esc/esc_register_reader.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>


void testReadsUint16Register()
{
    std::string executed_command;

    EscRegisterReader reader(
        [&](const std::string& command)
        {
            executed_command = command;

            return CommandResult{
                0,
                "4660\n"
            };
        });

    const RegisterReadResult result =
        reader.readU16(
            0,
            3,
            0x0110);

    assert(
        executed_command ==
        "ethercat reg_read "
        "-m 0 "
        "-p 3 "
        "-t uint16 "
        "0x0110");

    assert(result.success);
    assert(result.value == 0x1234);
    assert(result.error.empty());
}

void testReportsCommandFailure()
{
    EscRegisterReader reader(
        [](const std::string&)
        {
            return CommandResult{
                7,
                "Failed to read register\n"
            };
        });

    RegisterReadResult result;
    bool exception_thrown = false;

    try
    {
        result =
            reader.readU16(
                0,
                3,
                0x0110);
    }
    catch (...)
    {
        exception_thrown = true;
    }

    assert(!exception_thrown);
    assert(!result.success);
    assert(result.value == 0);

    assert(
        result.error.find("7") !=
        std::string::npos);
}

void testRejectsNonNumericOutput()
{
    EscRegisterReader reader([](const std::string&) {
        return CommandResult{0, "not-a-number\n"};
    });

    RegisterReadResult result;
    bool exception_thrown = false;

    try
    {
        result = reader.readU16(0, 3, 0x0110);
    }
    catch (...)
    {
        exception_thrown = true;
    }

    assert(!exception_thrown);
    assert(!result.success);
    assert(result.value == 0);
    assert(!result.error.empty());
}

void testRejectsValueAboveUint16Range()
{
    EscRegisterReader reader([](const std::string&) {
        return CommandResult{0, "65536\n"};
    });

    const RegisterReadResult result =
        reader.readU16(0, 3, 0x0110);

    assert(!result.success);
    assert(result.value == 0);
    assert(!result.error.empty());
}

void testRejectsTrailingGarbage()
{
    EscRegisterReader reader([](const std::string&) {
        return CommandResult{0, "4660garbage\n"};
    });

    const RegisterReadResult result =
        reader.readU16(0, 3, 0x0110);

    assert(!result.success);
    assert(result.value == 0);
    assert(!result.error.empty());
}

void testReadsHexadecimalOutput()
{
    EscRegisterReader reader([](const std::string&) {
        return CommandResult{0, "0x1234\n"};
    });

    const RegisterReadResult result =
        reader.readU16(0, 3, 0x0110);

    assert(result.success);
    assert(result.value == 0x1234);
    assert(result.error.empty());
}

void testRejectsNegativeRegisterValue()
{
    EscRegisterReader reader([](const std::string&) {
        return CommandResult{0, "-1\n"};
    });

    const RegisterReadResult result =
        reader.readU16(0, 3, 0x0110);

    assert(!result.success);
    assert(result.value == 0);
    assert(!result.error.empty());
}

void testRejectsEmptyOutput()
{
    EscRegisterReader reader([](const std::string&) {
        return CommandResult{0, ""};
    });

    const RegisterReadResult result =
        reader.readU16(0, 3, 0x0110);

    assert(!result.success);
    assert(result.value == 0);
    assert(!result.error.empty());
}

void testRejectsWhitespaceOnlyOutput()
{
    EscRegisterReader reader([](const std::string&) {
        return CommandResult{0, "   \n\t"};
    });

    const RegisterReadResult result =
        reader.readU16(0, 3, 0x0110);

    assert(!result.success);
    assert(result.value == 0);
    assert(!result.error.empty());
}

void testRejectsNegativeMasterIndex()
{
    int execution_count = 0;

    EscRegisterReader reader(
        [&](const std::string&) {
            ++execution_count;
            return CommandResult{0, "4660\n"};
        }
    );

    const RegisterReadResult result =
        reader.readU16(-1, 3, 0x0110);

    assert(!result.success);
    assert(result.value == 0);
    assert(!result.error.empty());
    assert(execution_count == 0);
}

void testRejectsNegativeSlavePosition()
{
    int execution_count = 0;

    EscRegisterReader reader(
        [&](const std::string&) {
            ++execution_count;
            return CommandResult{0, "4660\n"};
        }
    );

    const RegisterReadResult result =
        reader.readU16(0, -1, 0x0110);

    assert(!result.success);
    assert(result.value == 0);
    assert(!result.error.empty());
    assert(execution_count == 0);
}

void testRejectsEmptyCommandExecutor()
{
    EscRegisterReader reader(
        EscRegisterReader::CommandExecutor{}
    );

    RegisterReadResult result;
    bool exception_thrown = false;

    try
    {
        result = reader.readU16(0, 3, 0x0110);
    }
    catch (...)
    {
        exception_thrown = true;
    }

    assert(!exception_thrown);
    assert(!result.success);
    assert(result.value == 0);
    assert(!result.error.empty());
}

void testReadsIghDualNumericOutput()
{
    EscRegisterReader reader([](const std::string&) {
        return CommandResult{
            0,
            "0x5613 22035\n"
        };
    });

    const RegisterReadResult result =
        reader.readU16(0, 3, 0x0110);

    assert(result.success);
    assert(result.value == 0x5613);
    assert(result.error.empty());
}

void testRejectsInconsistentIghDualNumericOutput()
{
    EscRegisterReader reader([](const std::string&) {
        return CommandResult{
            0,
            "0x5613 22034\n"
        };
    });

    const RegisterReadResult result =
        reader.readU16(0, 3, 0x0110);

    assert(!result.success);
    assert(result.value == 0);
    assert(!result.error.empty());
}

int main()
{
    testReadsUint16Register();
    testReportsCommandFailure();
    testRejectsNonNumericOutput();
    testRejectsValueAboveUint16Range();
    testRejectsTrailingGarbage();
    testReadsHexadecimalOutput();
    testRejectsNegativeRegisterValue();
    testRejectsEmptyOutput();
    testRejectsWhitespaceOnlyOutput();
    testRejectsNegativeMasterIndex();
    testRejectsNegativeSlavePosition();
    testRejectsEmptyCommandExecutor();
    testReadsIghDualNumericOutput();
    testRejectsInconsistentIghDualNumericOutput();

    std::cout
        << "All ESC register reader tests passed\n";

    return 0;
}
