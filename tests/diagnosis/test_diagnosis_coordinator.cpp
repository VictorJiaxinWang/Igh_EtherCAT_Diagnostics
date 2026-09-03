#include "ethercat_diag/diagnosis/diagnosis_coordinator.h"

#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <optional>
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

SlaveSnapshot makeSlave(int position)
{
    return {
        position,
        1000,
        position,
        AlState::OP,
        false,
        true,
        "Test slave"
    };
}

NetworkSnapshot makeSnapshot(
    std::initializer_list<int> positions)
{
    NetworkSnapshot snapshot;
    snapshot.master.link_up = true;
    snapshot.master.slave_count =
        static_cast<int>(positions.size());

    for (const int position : positions)
    {
        snapshot.slaves.push_back(
            makeSlave(position));
    }

    return snapshot;
}

FaultEvent makeEvent(
    EventType type,
    int slave_position,
    std::uint64_t timestamp_ms = 1000)
{
    return {
        timestamp_ms,
        type,
        slave_position,
        1,
        0,
        "Test event"
    };
}

EscDiagnosticReader makeSuccessfulReader(
    std::vector<std::string>& commands)
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

    return EscDiagnosticReader(
        std::move(register_reader));
}

DiagnosisCoordinator makeCoordinator(
    std::vector<std::string>& commands,
    std::uint64_t cooldown_ms = 10000)
{
    ActiveDiagnosis diagnosis(
        0,
        makeSuccessfulReader(commands));

    return DiagnosisCoordinator(
        std::move(diagnosis),
        cooldown_ms);
}

void testIgnoresEventsWithoutSlaveLoss()
{
    std::vector<std::string> commands;
    DiagnosisCoordinator coordinator =
        makeCoordinator(commands);

    const NetworkSnapshot previous =
        makeSnapshot({0, 1, 2, 3});
    const NetworkSnapshot current =
        makeSnapshot({0, 1, 2, 3});

    const std::optional<DiagResult> result =
        coordinator.process(
            previous,
            current,
            {makeEvent(
                EventType::SLAVE_STATE_CHANGED,
                3)});

    require(!result.has_value(),
            "non-loss event does not trigger diagnosis");
    require(commands.empty(),
            "non-loss event performs no ESC reads");
}

void testDiagnosesLocatedBoundaryOnSlaveLoss()
{
    std::vector<std::string> commands;
    DiagnosisCoordinator coordinator =
        makeCoordinator(commands);

    const NetworkSnapshot previous =
        makeSnapshot({0, 1, 2, 3});
    const NetworkSnapshot current =
        makeSnapshot({0, 1, 2});

    const std::optional<DiagResult> result =
        coordinator.process(
            previous,
            current,
            {makeEvent(EventType::SLAVE_LOST, 3)});

    require(result.has_value(),
            "slave loss returns diagnosis result");
    require(result->success(),
            "located boundary diagnosis succeeds");
    require(result->boundary.valid,
            "located boundary is valid");
    require(result->boundary.last_alive_slave == 2,
            "last alive slave is located");
    require(result->boundary.first_lost_slave == 3,
            "first lost slave is located");
    require(commands.size() == 3,
            "one diagnosis reads three registers");

    for (const std::string& command : commands)
    {
        require(
            command.find("-m 0 -p 2") !=
                std::string::npos,
            "diagnosis reads the last alive slave");
    }
}

void testMultipleLossEventsTriggerOneDiagnosis()
{
    std::vector<std::string> commands;
    DiagnosisCoordinator coordinator =
        makeCoordinator(commands);

    const NetworkSnapshot previous =
        makeSnapshot({0, 1, 2, 3, 4});
    const NetworkSnapshot current =
        makeSnapshot({0, 1, 2});

    const std::optional<DiagResult> result =
        coordinator.process(
            previous,
            current,
            {
                makeEvent(EventType::SLAVE_LOST, 3),
                makeEvent(EventType::SLAVE_LOST, 4)
            });

    require(result.has_value(),
            "multiple losses still return a result");
    require(commands.size() == 3,
            "multiple losses trigger only one burst");
}

void testMissingPreviousSnapshotReturnsRejectedResult()
{
    std::vector<std::string> commands;
    DiagnosisCoordinator coordinator =
        makeCoordinator(commands);

    const std::optional<DiagResult> result =
        coordinator.process(
            std::nullopt,
            makeSnapshot({0, 1, 2}),
            {makeEvent(EventType::SLAVE_LOST, 3)});

    require(result.has_value(),
            "trigger without history returns a result");
    require(!result->attempted(),
            "trigger without history performs no diagnosis");
    require(!result->error.empty(),
            "trigger without history explains rejection");
    require(commands.empty(),
            "trigger without history performs no ESC reads");
}

void testFirstSlaveLossCannotReadLastAliveSlave()
{
    std::vector<std::string> commands;
    DiagnosisCoordinator coordinator =
        makeCoordinator(commands);

    const std::optional<DiagResult> result =
        coordinator.process(
            makeSnapshot({0, 1}),
            makeSnapshot({}),
            {makeEvent(EventType::SLAVE_LOST, 0)});

    require(result.has_value(),
            "first slave loss returns a result");
    require(result->boundary.valid,
            "master-to-first-slave boundary is located");
    require(result->boundary.last_alive_slave == -1,
            "master side has no readable slave");
    require(!result->attempted(),
            "missing last alive slave is not read");
    require(!result->error.empty(),
            "missing last alive slave explains rejection");
    require(commands.empty(),
            "first slave loss performs no ESC reads");
}

void testCooldownSuppressesRepeatedDiagnosisBursts()
{
    std::vector<std::string> commands;
    DiagnosisCoordinator coordinator =
        makeCoordinator(commands, 10000);

    const auto first = coordinator.process(
        makeSnapshot({0, 1, 2, 3, 4}),
        makeSnapshot({0, 1, 2, 3}),
        {makeEvent(EventType::SLAVE_LOST, 4, 2000)});

    const auto suppressed = coordinator.process(
        makeSnapshot({0, 1, 2, 3}),
        makeSnapshot({0, 1, 2}),
        {makeEvent(EventType::SLAVE_LOST, 3, 5000)});

    const auto after_cooldown = coordinator.process(
        makeSnapshot({0, 1, 2}),
        makeSnapshot({0, 1}),
        {makeEvent(EventType::SLAVE_LOST, 2, 12000)});

    require(first.has_value(),
            "first fault starts diagnosis burst");
    require(!suppressed.has_value(),
            "fault inside cooldown is suppressed");
    require(after_cooldown.has_value(),
            "fault at cooldown boundary starts a new burst");
    require(commands.size() == 6,
            "two allowed bursts read three registers each");
}

void testRecoveryResetsCooldownForNextFaultEpisode()
{
    std::vector<std::string> commands;
    DiagnosisCoordinator coordinator =
        makeCoordinator(commands, 10000);

    coordinator.process(
        makeSnapshot({0, 1, 2, 3}),
        makeSnapshot({0, 1, 2}),
        {makeEvent(EventType::SLAVE_LOST, 3, 2000)});

    coordinator.resetCooldown();

    const auto next_episode = coordinator.process(
        makeSnapshot({0, 1, 2}),
        makeSnapshot({0, 1}),
        {makeEvent(EventType::SLAVE_LOST, 2, 3000)});

    require(next_episode.has_value(),
            "recovery allows immediate diagnosis of a new fault");
    require(commands.size() == 6,
            "new fault after recovery runs another burst");
}

void testRejectedDiagnosisDoesNotStartCooldown()
{
    std::vector<std::string> commands;
    DiagnosisCoordinator coordinator =
        makeCoordinator(commands, 10000);

    const auto rejected = coordinator.process(
        makeSnapshot({0, 1}),
        makeSnapshot({}),
        {makeEvent(EventType::SLAVE_LOST, 0, 2000)});

    const auto valid = coordinator.process(
        makeSnapshot({0, 1, 2}),
        makeSnapshot({0, 1}),
        {makeEvent(EventType::SLAVE_LOST, 2, 3000)});

    require(rejected.has_value() &&
                !rejected->attempted(),
            "unreadable boundary returns rejected result");
    require(valid.has_value() && valid->attempted(),
            "rejected result does not suppress next real diagnosis");
    require(commands.size() == 3,
            "only the valid diagnosis reads ESC registers");
}

void testClockRollbackDoesNotCreatePermanentCooldown()
{
    std::vector<std::string> commands;
    DiagnosisCoordinator coordinator =
        makeCoordinator(commands, 10000);

    coordinator.process(
        makeSnapshot({0, 1, 2, 3}),
        makeSnapshot({0, 1, 2}),
        {makeEvent(EventType::SLAVE_LOST, 3, 5000)});

    const auto after_rollback = coordinator.process(
        makeSnapshot({0, 1, 2}),
        makeSnapshot({0, 1}),
        {makeEvent(EventType::SLAVE_LOST, 2, 3000)});

    require(after_rollback.has_value(),
            "clock rollback permits a new diagnosis burst");
    require(commands.size() == 6,
            "clock rollback does not underflow cooldown elapsed time");
}

} // namespace

int main()
{
    testIgnoresEventsWithoutSlaveLoss();
    testDiagnosesLocatedBoundaryOnSlaveLoss();
    testMultipleLossEventsTriggerOneDiagnosis();
    testMissingPreviousSnapshotReturnsRejectedResult();
    testFirstSlaveLossCannotReadLastAliveSlave();
    testCooldownSuppressesRepeatedDiagnosisBursts();
    testRecoveryResetsCooldownForNextFaultEpisode();
    testRejectedDiagnosisDoesNotStartCooldown();
    testClockRollbackDoesNotCreatePermanentCooldown();

    std::cout
        << "All diagnosis coordinator tests passed\n";
    return 0;
}
