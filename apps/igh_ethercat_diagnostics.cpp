#include "ethercat_diag/monitoring/ethercat_reader.h"
#include "ethercat_diag/monitoring/monitor.h"
#include "ethercat_diag/detection/event_detector.h"
#include "ethercat_diag/detection/recovery_tracker.h"
#include "ethercat_diag/recording/blackbox.h"
#include "ethercat_diag/recording/blackbox_filename.h"
#include "ethercat_diag/diagnosis/diagnosis_coordinator.h"
#include "ethercat_diag/diagnosis/diagnosis_result_formatter.h"

#include <optional>
#include <filesystem>
#include <system_error>
#include <iostream>
#include <vector>
#include <csignal>
#include <iomanip>

namespace
{

volatile std::sig_atomic_t stop_requested = 0;

void handleStopSignal(int)
{
    stop_requested = 1;
}

const char* alStateToString(AlState state)
{
    switch (state)
    {
    case AlState::INIT:
        return "INIT";

    case AlState::PREOP:
        return "PREOP";

    case AlState::SAFEOP:
        return "SAFEOP";

    case AlState::OP:
        return "OP";

    case AlState::UNKNOWN:
        return "UNKNOWN";
    }

    return "UNKNOWN";
}

void printSnapshot(const NetworkSnapshot& snapshot)
{
    const MasterSnapshot& master = snapshot.master;
    const std::vector<SlaveSnapshot>& slaves = snapshot.slaves;

    std::cout << "=====================================" << '\n';
    std::cout << "Master0\n";
    std::cout << "Phase  : " << master.phase << '\n';
    std::cout << "Active : "
              << (master.active ? "yes" : "no")
              << '\n';
    std::cout << "Link   : "
              << (master.link_up ? "UP" : "DOWN")
              << '\n';
    std::cout << "Slaves : "
              << master.slave_count
              << "\n\n";

    for (const SlaveSnapshot& slave : slaves)
    {
        std::cout
            << "Slave"
            << slave.position
            << " : "
            << alStateToString(slave.state);

        if (slave.has_error)
        {
            std::cout << " ERROR";
        }

        std::cout
            << "  "
            << slave.name
            << '\n';
    }
}

const char* eventTypeToString(EventType type)
{
    switch (type)
    {
    case EventType::MASTER_LINK_DOWN:
        return "MASTER_LINK_DOWN";

    case EventType::MASTER_LINK_UP:
        return "MASTER_LINK_UP";

    case EventType::SLAVE_COUNT_CHANGED:
        return "SLAVE_COUNT_CHANGED";

    case EventType::SLAVE_LOST:
        return "SLAVE_LOST";

    case EventType::SLAVE_STATE_CHANGED:
        return "SLAVE_STATE_CHANGED";
    }

    return "UNKNOWN_EVENT";
}

void printFaultEvent(const FaultEvent& event)
{
    std::cout
        << "[EVENT]"
        << " timestamp=" << event.timestamp_ms
        << " type=" << eventTypeToString(event.type);

    if (event.slave_position >= 0)
    {
        std::cout
            << " slave=" << event.slave_position;
    }

    std::cout
        << " old=" << event.old_value
        << " new=" << event.new_value
        << " description=\"" << event.description
        << "\"\n";
}

void printRecoveryEvent(const RecoveryEvent& event)
{
    std::cout
        << "[RECOVERY]"
        << " timestamp=" << event.timestamp_ms
        << " description=\"" << event.description << '"'
        << " slaves=" << event.fault_slave_count
        << "->" << event.recovered_slave_count
        << " duration=" << std::fixed << std::setprecision(3)
        << static_cast<double>(event.duration_ms) / 1000.0
        << "s";

    if (!event.recovered_slave_positions.empty())
    {
        std::cout << " recovered_positions=";

        for (std::size_t index = 0;
             index < event.recovered_slave_positions.size();
             ++index)
        {
            if (index != 0)
            {
                std::cout << ',';
            }

            std::cout << event.recovered_slave_positions[index];
        }
    }

    std::cout << '\n';
}

}

int main()
{
    if (std::signal(SIGINT, handleStopSignal) == SIG_ERR ||
    std::signal(SIGTERM, handleStopSignal) == SIG_ERR) 
    {
        std::cerr << "Failed to install signal handler\n";
        return 1;
    }

    EthercatReader reader;
    EventDetector detector;
    RecoveryTracker recovery_tracker;
    std::optional<NetworkSnapshot> previous_snapshot;
    
    Blackbox blackbox(30, 10);  // 当前1Hz，故障前30s，故障后10s

    const std::filesystem::path log_directory =
        std::filesystem::current_path() /
        "logs";

    std::error_code directory_error;

    std::filesystem::create_directories(
        log_directory,
        directory_error);

    if (directory_error)
    {
        std::cerr
            << "Failed to create log directory: "
            << directory_error.message()
            << '\n';

        return 1;
    }

    bool blackbox_save_attempted = false;

    MonitorConfig config;
    config.master_index = 0;
    config.interval = std::chrono::milliseconds(1000);

    DiagnosisCoordinator diagnosis_coordinator(
        config.master_index);

    Monitor monitor(
        config,

        [&reader](int master_index,
                NetworkSnapshot& snapshot) {
            const bool success =
                reader.readSnapshot(master_index, snapshot);

            if (!success) {
                std::cerr
                    << "[WARN] Failed to read EtherCAT master "
                    << master_index << '\n';
            }

            return success;
        },

          [&detector,
          &recovery_tracker,
          &diagnosis_coordinator,
          &previous_snapshot,
          &blackbox,
          &log_directory,
          &blackbox_save_attempted]
          (const NetworkSnapshot& snapshot)
          {
              printSnapshot(snapshot);

            blackbox.push(snapshot);

            const std::vector<FaultEvent> events =
                detector.process(snapshot);

            for (const FaultEvent& event : events)
            {
                  printFaultEvent(event);
                  blackbox.trigger(event);
              }

              const std::optional<DiagResult>
                  diagnosis_result =
                      diagnosis_coordinator.process(
                          previous_snapshot,
                          snapshot,
                          events);

              if (diagnosis_result)
              {
                  std::cout
                      << formatDiagnosisResult(
                             *diagnosis_result)
                      << '\n';
              }

              const std::optional<RecoveryEvent> recovery =
                  recovery_tracker.process(
                      previous_snapshot,
                      snapshot,
                      events);

              if (recovery)
              {
                  printRecoveryEvent(*recovery);
                  diagnosis_coordinator.resetCooldown();
              }

            previous_snapshot = snapshot;
            
            if (blackbox.state() ==
                    BlackboxState::SAVING &&
                !blackbox_save_attempted)
            {
                blackbox_save_attempted = true;

                const auto& trigger_event =
                    blackbox.triggerEvent();

                if (!trigger_event)
                {
                    std::cerr
                        << "[BLACKBOX] Missing trigger event\n";
                    return;
                }

                const std::filesystem::path file_path =
                    makeUniqueBlackboxPath(
                        log_directory,
                        trigger_event->timestamp_ms);

                if (blackbox.saveToFile(
                        file_path.string()))
                {
                    std::cout
                        << "[BLACKBOX] Saved "
                        << blackbox.size()
                        << " snapshots to "
                        << file_path.string()
                        << '\n';
                }
                else
                {
                    std::cerr
                        << "[BLACKBOX] Failed to save "
                        << file_path.string()
                        << '\n';
                }
            }
        }
    );

    std::cout << "EtherCAT monitor started. Press Ctrl+C to stop.\n";

    const bool success = monitor.run([]() {
        return stop_requested == 0;
    });

    if (!success) {
        std::cerr << "Monitor configuration is invalid\n";
        return 1;
    }

    std::cout << "EtherCAT monitor stopped\n";

    return 0;
}
