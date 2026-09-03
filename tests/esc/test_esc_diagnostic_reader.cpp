#include "ethercat_diag/esc/esc_diagnostic_reader.h"

#include <cassert>
#include <string>
#include <vector>
#include <iostream>

void testReadsAllDiagnosticRegisters()
{
    std::vector<std::string> commands;

    EscRegisterReader register_reader(
        [&](const std::string& command) {
            commands.push_back(command);

            if (command.find("0x0110") != std::string::npos)
            {
                return CommandResult{0, "4369\n"};
            }

            if (command.find("0x0130") != std::string::npos)
            {
                return CommandResult{0, "2\n"};
            }

            if (command.find("0x0134") != std::string::npos)
            {
                return CommandResult{0, "3\n"};
            }

            return CommandResult{1, "unexpected address\n"};
        }
    );

    EscDiagnosticReader reader(register_reader);

    const EscDiagnosticSample sample =
        reader.read(0, 3);

    assert(sample.master_index == 0);
    assert(sample.slave_position == 3);

    assert(sample.dl_status.success);
    assert(sample.dl_status.value == 0x1111);

    assert(sample.al_status.success);
    assert(sample.al_status.value == 0x0002);

    assert(sample.al_status_code.success);
    assert(sample.al_status_code.value == 0x0003);

    assert(sample.complete());
    assert(commands.size() == 3);
}

void testContinuesAfterMiddleRegisterFailure()
{
    std::vector<std::string> commands;

    EscRegisterReader register_reader(
        [&](const std::string& command) {
            commands.push_back(command);

            if (command.find("0x0110") != std::string::npos)
            {
                return CommandResult{0, "43690\n"};
            }

            if (command.find("0x0130") != std::string::npos)
            {
                return CommandResult{
                    7,
                    "AL status read failed\n"
                };
            }

            if (command.find("0x0134") != std::string::npos)
            {
                return CommandResult{0, "27\n"};
            }

            return CommandResult{
                1,
                "unexpected address\n"
            };
        }
    );

    EscDiagnosticReader reader(register_reader);

    const EscDiagnosticSample sample =
        reader.read(0, 3);

    assert(sample.dl_status.success);
    assert(sample.dl_status.value == 0xAAAA);

    assert(!sample.al_status.success);
    assert(sample.al_status.value == 0);
    assert(!sample.al_status.error.empty());

    assert(sample.al_status_code.success);
    assert(sample.al_status_code.value == 0x001B);

    assert(!sample.complete());
    assert(commands.size() == 3);
}

void testPreservesAllRegisterFailures()
{
    std::vector<std::string> commands;

    EscRegisterReader register_reader(
        [&](const std::string& command) {
            commands.push_back(command);

            if (command.find("0x0110") != std::string::npos)
            {
                return CommandResult{
                    11,
                    "DL status failed\n"
                };
            }

            if (command.find("0x0130") != std::string::npos)
            {
                return CommandResult{
                    12,
                    "AL status failed\n"
                };
            }

            if (command.find("0x0134") != std::string::npos)
            {
                return CommandResult{
                    13,
                    "AL status code failed\n"
                };
            }

            return CommandResult{
                99,
                "unexpected address\n"
            };
        }
    );

    EscDiagnosticReader reader(register_reader);

    const EscDiagnosticSample sample =
        reader.read(0, 3);

    assert(!sample.dl_status.success);
    assert(sample.dl_status.value == 0);
    assert(
        sample.dl_status.error.find("11")
        != std::string::npos
    );

    assert(!sample.al_status.success);
    assert(sample.al_status.value == 0);
    assert(
        sample.al_status.error.find("12")
        != std::string::npos
    );

    assert(!sample.al_status_code.success);
    assert(sample.al_status_code.value == 0);
    assert(
        sample.al_status_code.error.find("13")
        != std::string::npos
    );

    assert(!sample.complete());
    assert(commands.size() == 3);
}

int main()
{
    testReadsAllDiagnosticRegisters();
    testContinuesAfterMiddleRegisterFailure();
    testPreservesAllRegisterFailures();

    std::cout << "All ESC register reader tests passed\n";
    return 0;
}
