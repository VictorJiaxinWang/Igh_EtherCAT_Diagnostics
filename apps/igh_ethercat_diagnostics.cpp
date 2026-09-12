#include "ethercat_diag/monitoring/ioctl_monitor_adapter.h"
#include "ethercat_diag/monitoring/ioctl_snapshot_reader.h"
#include "ethercat_diag/monitoring/monitor.h"
#include "ethercat_diag/monitoring/port_error_monitor.h"
#include "ethercat_diag/detection/event_detector.h"
#include "ethercat_diag/detection/recovery_tracker.h"
#include "ethercat_diag/recording/blackbox.h"
#include "ethercat_diag/recording/blackbox_filename.h"
#include "ethercat_diag/diagnosis/diagnosis_coordinator.h"
#include "ethercat_diag/diagnosis/diagnosis_result_formatter.h"
#include "ethercat_diag/infrastructure/runtime_output.h"
#include "ethercat_diag/root_cause/evidence_builder.h"
#include "ethercat_diag/root_cause/evidence_window.h"
#include "ethercat_diag/root_cause/root_cause_analyzer.h"
#include "ethercat_diag/root_cause/root_cause_formatter.h"
#include "ethercat_diag/publishing/web_data_publisher.h"
#include "ethercat_diag/cli/daemon_options.h"

#include <optional>
#include <filesystem>
#include <system_error>
#include <iostream>
#include <vector>
#include <csignal>
#include <cstdint>
#include <iomanip>
#include <unordered_map>
#include <map>
#include <memory>
#include <string>

namespace
{

volatile std::sig_atomic_t stop_requested = 0;

struct MasterRuntime
{
    explicit MasterRuntime(int index)
        : diagnosis_coordinator(index),
          port_error_monitor(index),
          blackbox(30, 10)
    {
    }

    std::uint64_t consecutive_read_failures{};
    std::unordered_map<int, std::uint64_t> port_read_failures;
    EventDetector detector;
    RecoveryTracker recovery_tracker;
    std::optional<NetworkSnapshot> previous_snapshot;
    EvidenceBuilder evidence_builder;
    EvidenceWindow evidence_window{10000U};
    RootCauseAnalyzer root_cause_analyzer;
    std::optional<RootCauseReport> latest_root_cause;
    std::optional<RootCauseReport> active_root_cause;
    std::optional<std::uint64_t> last_fault_timestamp_ms;
    DiagnosisCoordinator diagnosis_coordinator;
    PortErrorMonitor port_error_monitor;
    Blackbox blackbox;
    bool blackbox_save_attempted{};
};

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
    std::cout << "Master" << master.master_index << '\n';
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
            << slave.position << " (alias " << slave.alias << ':'
            << slave.relative_position << ')'
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

    case EventType::PORT_INVALID_FRAME_INCREASED:
        return "PORT_INVALID_FRAME_INCREASED";

    case EventType::PORT_RX_ERROR_INCREASED:
        return "PORT_RX_ERROR_INCREASED";

    case EventType::PORT_FORWARDED_RX_ERROR_INCREASED:
        return "PORT_FORWARDED_RX_ERROR_INCREASED";

    case EventType::PORT_LOST_LINK_INCREASED:
        return "PORT_LOST_LINK_INCREASED";
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

    std::cout << " master=" << event.master_index;
    if (event.slave_alias > 0)
    {
        std::cout << " alias=" << event.slave_alias << ':'
                  << event.slave_relative_position;
    }

    if (event.port_position >= 0)
    {
        std::cout
            << " port=" << event.port_position;
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

int main(int argc, char* argv[])
{

    std::vector<int> configured_masters{0};
    if (argc == 3 && std::string(argv[1]) == "--masters")
    {
        configured_masters = parseMasterList(argv[2]);
    }
    else if (argc != 1)
    {
        std::cerr << "Usage: " << argv[0] << " [--masters 0,1]\n";
        return 2;
    }
    if (configured_masters.empty())
    {
        std::cerr << "Invalid --masters value\n";
        return 2;
    }

    enableImmediateFlush(std::cout);

    if (std::signal(SIGINT, handleStopSignal) == SIG_ERR ||
    std::signal(SIGTERM, handleStopSignal) == SIG_ERR) 
    {
        std::cerr << "Failed to install signal handler\n";
        return 1;
    }

    IoctlSnapshotReader reader;
    std::map<int, std::unique_ptr<MasterRuntime>> runtimes;

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

    for (const int master_index : configured_masters)
    {
        std::filesystem::create_directories(
            log_directory / ("master" + std::to_string(master_index)),
            directory_error);
        if (directory_error)
        {
            std::cerr << "Failed to create per-master log directory: "
                      << directory_error.message() << '\n';
            return 1;
        }
        runtimes.emplace(master_index,
            std::make_unique<MasterRuntime>(master_index));
    }

    WebDataPublisher web_data_publisher(log_directory);

    MonitorConfig config;
    config.master_indices = configured_masters;
    config.interval = std::chrono::milliseconds(1000);

    Monitor monitor(
        config,

        [&reader, &runtimes](int master_index,
                NetworkSnapshot& snapshot) {
            MasterRuntime& runtime = *runtimes.at(master_index);
            IghDeviceError error;
            const bool success = applySnapshotReadResult(
                reader.readSnapshot(master_index),
                snapshot,
                error);

            if (success)
            {
                runtime.consecutive_read_failures = 0U;
                return true;
            }

            ++runtime.consecutive_read_failures;

            if (runtime.consecutive_read_failures == 1U ||
                runtime.consecutive_read_failures % 60U == 0U)
            {
                std::cerr
                    << "[WARN] Failed to read EtherCAT master "
                    << master_index
                    << " operation=" << error.operation
                    << " errno=" << error.system_errno
                    << " message=\"" << error.message << "\""
                    << " consecutive=" << runtime.consecutive_read_failures
                    << '\n';
            }

            return false;
        },

          [&runtimes,
          &web_data_publisher,
          &log_directory]
          (const NetworkSnapshot& snapshot)
          {
            MasterRuntime& runtime =
                *runtimes.at(snapshot.master.master_index);
              printSnapshot(snapshot);

            runtime.blackbox.push(snapshot);

            std::vector<FaultEvent> events =
                runtime.detector.process(snapshot);

            PortErrorMonitorResult port_result =
                runtime.port_error_monitor.process(snapshot);

            if (!port_result.error.empty())
            {
                std::uint64_t& failure_count =
                    runtime.port_read_failures[
                        port_result.slave_position];
                ++failure_count;

                if (failure_count == 1U ||
                    failure_count % 60U == 0U)
                {
                    std::cerr
                        << "[WARN] Failed to read port counters"
                        << " slave=" << port_result.slave_position
                        << " message=\"" << port_result.error << "\""
                        << " consecutive="
                        << failure_count
                        << '\n';
                }
            }
            else if (port_result.attempted)
            {
                runtime.port_read_failures.erase(
                    port_result.slave_position);
                events.insert(
                    events.end(),
                    port_result.events.begin(),
                    port_result.events.end());
            }

            for (const FaultEvent& event : events)
            {
                  printFaultEvent(event);
                  runtime.blackbox.trigger(event);

                  if (event.type != EventType::MASTER_LINK_UP)
                  {
                      runtime.last_fault_timestamp_ms = event.timestamp_ms;
                  }
              }

              std::string publish_error;
              if (!events.empty() &&
                  !web_data_publisher.appendFaultEvents(
                      events,
                      publish_error))
              {
                  std::cerr
                      << "[WEB_DATA] " << publish_error << '\n';
              }

              const std::optional<DiagResult>
                  diagnosis_result =
                      runtime.diagnosis_coordinator.process(
                          runtime.previous_snapshot,
                          snapshot,
                          events);

              if (diagnosis_result)
              {
                  std::cout
                      << formatDiagnosisResult(
                             *diagnosis_result)
                      << '\n';
              }

              DiagnosisEvidence evidence =
                  runtime.evidence_builder.build(
                      snapshot,
                      events,
                      diagnosis_result);
              runtime.evidence_window.push(evidence);

              bool root_cause_triggered = false;
              for (const FaultEvent& event : events)
              {
                  if (event.type != EventType::MASTER_LINK_UP)
                  {
                      root_cause_triggered = true;
                      break;
                  }
              }

              if (root_cause_triggered)
              {
                  std::vector<DiagnosisEvidence> related;
                  if (evidence.boundary)
                  {
                      related = runtime.evidence_window.relatedToBoundary(
                          *evidence.boundary);
                  }
                  else
                  {
                      related.assign(
                          runtime.evidence_window.recent().begin(),
                          runtime.evidence_window.recent().end());
                  }

                  runtime.latest_root_cause =
                      runtime.root_cause_analyzer.analyze(related);
                  runtime.active_root_cause = runtime.latest_root_cause;
                  std::cout
                      << formatRootCauseReport(
                             *runtime.latest_root_cause)
                      << '\n';

                  if (!web_data_publisher.appendRootCause(
                          snapshot.master.master_index,
                          *runtime.latest_root_cause,
                          publish_error))
                  {
                      std::cerr
                          << "[WEB_DATA] " << publish_error << '\n';
                  }
              }

              const std::optional<RecoveryEvent> recovery =
                  runtime.recovery_tracker.process(
                      runtime.previous_snapshot,
                      snapshot,
                      events);

              if (recovery)
              {
                  printRecoveryEvent(*recovery);
                  runtime.diagnosis_coordinator.resetCooldown();
                  runtime.active_root_cause.reset();

                  if (!web_data_publisher.appendRecovery(
                          snapshot.master.master_index,
                          *recovery,
                          publish_error))
                  {
                      std::cerr
                          << "[WEB_DATA] " << publish_error << '\n';
                  }
              }

            runtime.previous_snapshot = snapshot;

            std::vector<PublishedMasterStatus> statuses;
            for (const auto& [master_index, state] : runtimes)
            {
                (void)master_index;
                if (state->previous_snapshot)
                {
                    statuses.push_back(PublishedMasterStatus{
                        *state->previous_snapshot,
                        state->last_fault_timestamp_ms,
                        state->active_root_cause});
                }
            }
            if (!web_data_publisher.publishStatus(statuses, publish_error))
            {
                std::cerr << "[WEB_DATA] " << publish_error << '\n';
            }
            
            if (runtime.blackbox.state() ==
                    BlackboxState::SAVING &&
                !runtime.blackbox_save_attempted)
            {
                runtime.blackbox_save_attempted = true;

                const auto& trigger_event =
                    runtime.blackbox.triggerEvent();

                if (!trigger_event)
                {
                    std::cerr
                        << "[BLACKBOX] Missing trigger event\n";
                    return;
                }

                const std::filesystem::path file_path =
                    makeUniqueBlackboxPath(
                        log_directory / ("master" +
                            std::to_string(snapshot.master.master_index)),
                        trigger_event->timestamp_ms);

                if (runtime.blackbox.saveToFile(
                        file_path.string()))
                {
                    bool root_cause_saved = true;
                    if (runtime.latest_root_cause)
                    {
                        root_cause_saved =
                            appendRootCauseReportJsonl(
                                file_path.string(),
                                *runtime.latest_root_cause);
                    }

                    std::cout
                        << "[BLACKBOX] Saved "
                        << runtime.blackbox.size()
                        << " snapshots to "
                        << file_path.string()
                        << '\n';

                    if (!root_cause_saved)
                    {
                        std::cerr
                            << "[BLACKBOX] Failed to append root cause to "
                            << file_path.string()
                            << '\n';
                    }
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
