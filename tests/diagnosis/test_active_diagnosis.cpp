#include "ethercat_diag/diagnosis/active_diagnosis.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

void require(bool condition, std::string_view message)
{
    if (condition)
    {
        return;
    }

    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

EscDiagnosticReader makeSuccessfulReader(
    std::vector<std::string>& commands
)
{
    EscRegisterReader register_reader(
        [&commands](const std::string& command) {
            commands.push_back(command);

            if (command.find("0x0110") != std::string::npos)
            {
                return CommandResult{0, "0x5613 22035\n"};
            }
            if (command.find("0x0130") != std::string::npos)
            {
                return CommandResult{0, "0x0002 2\n"};
            }
            if (command.find("0x0134") != std::string::npos)
            {
                return CommandResult{0, "0x0000 0\n"};
            }

            return CommandResult{1, "unexpected address\n"};
        }
    );

    return EscDiagnosticReader(std::move(register_reader));
}

void testReadsLastAliveSlave()
{
    std::vector<std::string> commands;
    ActiveDiagnosis diagnosis(0, makeSuccessfulReader(commands));

    const DiagResult result = diagnosis.run({3, 4, true});

    require(result.master_index == 0, "preserves master index");
    require(result.boundary.valid, "preserves valid boundary");
    require(result.boundary.last_alive_slave == 3, "preserves last alive slave");
    require(result.boundary.first_lost_slave == 4, "preserves first lost slave");
    require(result.attempted(), "valid boundary attempts diagnosis");
    require(result.success(), "complete register sample succeeds");
    require(result.sample->slave_position == 3, "reads last alive slave");
    require(commands.size() == 3, "reads exactly three ESC registers");

    for (const std::string& command : commands)
    {
        require(
            command.find("-m 0 -p 3") != std::string::npos,
            "every command targets master 0 slave 3"
        );
        require(
            command.find("-p 4") == std::string::npos,
            "never reads the first lost slave"
        );
    }
}

void testRejectsBoundaryMarkedInvalid()
{
    std::vector<std::string> commands;
    ActiveDiagnosis diagnosis(0, makeSuccessfulReader(commands));

    const DiagResult result = diagnosis.run({3, 4, false});

    require(!result.attempted(), "invalid boundary is not attempted");
    require(!result.success(), "invalid boundary does not succeed");
    require(!result.error.empty(), "invalid boundary explains rejection");
    require(commands.empty(), "invalid boundary executes no command");
}

void testRejectsNegativeBoundaryPositions()
{
    std::vector<std::string> commands;
    ActiveDiagnosis diagnosis(0, makeSuccessfulReader(commands));

    const DiagResult negative_alive = diagnosis.run({-1, 0, true});
    const DiagResult negative_lost = diagnosis.run({3, -1, true});

    require(!negative_alive.attempted(), "negative last alive is rejected");
    require(!negative_alive.error.empty(), "negative last alive explains rejection");
    require(!negative_lost.attempted(), "negative first lost is rejected");
    require(!negative_lost.error.empty(), "negative first lost explains rejection");
    require(commands.empty(), "negative boundary executes no command");
}

void testPreservesPartialRegisterFailure()
{
    std::vector<std::string> commands;
    EscRegisterReader register_reader(
        [&commands](const std::string& command) {
            commands.push_back(command);

            if (command.find("0x0130") != std::string::npos)
            {
                return CommandResult{7, "AL status read failed\n"};
            }
            return CommandResult{0, "0\n"};
        }
    );
    ActiveDiagnosis diagnosis(
        0,
        EscDiagnosticReader(std::move(register_reader))
    );

    const DiagResult result = diagnosis.run({3, 4, true});

    require(result.attempted(), "partial failure was attempted");
    require(!result.success(), "partial register failure does not succeed");
    require(result.sample.has_value(), "partial sample is preserved");
    require(!result.sample->al_status.success, "failed register is preserved");
    require(!result.sample->al_status.error.empty(), "register error is preserved");
    require(commands.size() == 3, "continues burst after one read failure");
}

void testUsesConfiguredMasterIndex()
{
    std::vector<std::string> commands;
    ActiveDiagnosis diagnosis(1, makeSuccessfulReader(commands));

    const DiagResult result = diagnosis.run({2, 3, true});

    require(result.success(), "configured master diagnosis succeeds");
    for (const std::string& command : commands)
    {
        require(
            command.find("-m 1 -p 2") != std::string::npos,
            "every command uses configured master and last alive slave"
        );
    }
}

void testRejectsNegativeMasterIndex()
{
    std::vector<std::string> commands;
    ActiveDiagnosis diagnosis(-1, makeSuccessfulReader(commands));

    const DiagResult result = diagnosis.run({3, 4, true});

    require(!result.attempted(), "negative master is not attempted");
    require(!result.error.empty(), "negative master explains rejection");
    require(commands.empty(), "negative master executes no command");
}

} // namespace

int main()
{
    testReadsLastAliveSlave();
    testRejectsBoundaryMarkedInvalid();
    testRejectsNegativeBoundaryPositions();
    testPreservesPartialRegisterFailure();
    testUsesConfiguredMasterIndex();
    testRejectsNegativeMasterIndex();

    std::cout << "All active diagnosis tests passed\n";
    return 0;
}
