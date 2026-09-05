#include "ethercat_diag/root_cause/root_cause_analyzer.h"

#include <algorithm>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace
{

RootCauseCandidate& candidate(
    std::vector<RootCauseCandidate>& candidates,
    RootCauseKind kind)
{
    const auto found = std::find_if(
        candidates.begin(),
        candidates.end(),
        [kind](const RootCauseCandidate& value) {
            return value.kind == kind;
        });
    return *found;
}

void support(
    RootCauseCandidate& value,
    int weight,
    std::string description)
{
    if (weight <= 0)
    {
        return;
    }
    value.score += weight;
    value.supporting_evidence.push_back(std::move(description));
}

void contradict(
    RootCauseCandidate& value,
    int weight,
    std::string description)
{
    if (weight <= 0)
    {
        return;
    }
    value.score -= weight;
    value.contradicting_evidence.push_back(std::move(description));
}

std::string portPrefix(const PortErrorEvidence& error)
{
    return "Slave" + std::to_string(error.slave_position) +
        " Port" + std::to_string(error.port_position) + " ";
}

std::string hex16(std::uint16_t value)
{
    std::ostringstream output;
    output << "0x" << std::hex << std::setw(4)
           << std::setfill('0') << value;
    return output.str();
}

int kindPriority(RootCauseKind kind)
{
    switch (kind)
    {
    case RootCauseKind::MASTER_LINK_FAILURE:
        return 0;
    case RootCauseKind::BOUNDARY_LINK_FAILURE:
        return 1;
    case RootCauseKind::SLAVE_INTERNAL_ERROR:
        return 2;
    case RootCauseKind::LINK_QUALITY_DEGRADATION:
        return 3;
    case RootCauseKind::UNKNOWN:
        return 4;
    }
    return 5;
}

void installActions(RootCauseCandidate& value)
{
    switch (value.kind)
    {
    case RootCauseKind::MASTER_LINK_FAILURE:
        value.recommended_actions = {
            "Check the master network interface and link LED",
            "Inspect the cable between the master and Slave0",
            "Verify the IgH master device attachment"};
        break;
    case RootCauseKind::BOUNDARY_LINK_FAILURE:
        value.recommended_actions = {
            "Inspect and reseat the cable at the reported boundary",
            "Check power and link LEDs on the first lost slave",
            "Compare boundary-port counters after reconnecting"};
        break;
    case RootCauseKind::SLAVE_INTERNAL_ERROR:
        value.recommended_actions = {
            "Decode the reported AL Status Code with the slave manual",
            "Check the slave application, watchdog and synchronization",
            "Record the state before resetting the slave"};
        break;
    case RootCauseKind::LINK_QUALITY_DEGRADATION:
        value.recommended_actions = {
            "Inspect shielding, grounding and connector quality",
            "Trend port counter deltas instead of cumulative values",
            "Replace the suspect cable if counters keep increasing"};
        break;
    case RootCauseKind::UNKNOWN:
        value.recommended_actions = {
            "Collect another fault window without restarting the daemon",
            "Check collection errors and device permissions",
            "Inspect the blackbox timeline manually"};
        break;
    }
}

} // namespace

RootCauseAnalyzer::RootCauseAnalyzer(
    RootCauseRuleWeights weights)
    : weights_(std::move(weights))
{
    const std::initializer_list<int> integer_weights = {
        weights_.master_link_down,
        weights_.slave_loss_master,
        weights_.master_up_master_contradiction,
        weights_.boundary_detected,
        weights_.slave_lost_boundary,
        weights_.master_up_boundary,
        weights_.master_down_boundary_contradiction,
        weights_.boundary_lost_link_per_delta,
        weights_.boundary_lost_link_cap,
        weights_.boundary_other_error_per_delta,
        weights_.boundary_other_error_cap,
        weights_.al_status_code_error,
        weights_.al_error_indication,
        weights_.slave_state_error,
        weights_.master_down_slave_contradiction,
        weights_.al_status_code_clear_contradiction,
        weights_.al_error_clear_contradiction,
        weights_.quality_error_per_delta,
        weights_.quality_error_cap,
        weights_.quality_lost_link_per_delta,
        weights_.quality_lost_link_cap,
        weights_.master_up_quality,
        weights_.no_slave_loss_quality,
        weights_.slave_loss_quality_contradiction,
        weights_.conclusive_min_score,
        weights_.conclusive_min_lead};

    if (std::any_of(
            integer_weights.begin(),
            integer_weights.end(),
            [](int value) { return value < 0; }) ||
        weights_.conclusive_min_confidence < 0.0 ||
        weights_.conclusive_min_confidence > 1.0)
    {
        throw std::invalid_argument(
            "root cause rule weights must be non-negative and confidence must be within [0, 1]");
    }
}

const RootCauseRuleWeights&
RootCauseAnalyzer::weights() const noexcept
{
    return weights_;
}

RootCauseReport RootCauseAnalyzer::analyze(
    const std::vector<DiagnosisEvidence>& evidence) const
{
    RootCauseReport report;
    report.candidates = {
        {RootCauseKind::MASTER_LINK_FAILURE, 0, {}, {}, {}},
        {RootCauseKind::BOUNDARY_LINK_FAILURE, 0, {}, {}, {}},
        {RootCauseKind::SLAVE_INTERNAL_ERROR, 0, {}, {}, {}},
        {RootCauseKind::LINK_QUALITY_DEGRADATION, 0, {}, {}, {}},
        {RootCauseKind::UNKNOWN, 0, {}, {}, {}}};

    auto& master = candidate(
        report.candidates,
        RootCauseKind::MASTER_LINK_FAILURE);
    auto& boundary_link = candidate(
        report.candidates,
        RootCauseKind::BOUNDARY_LINK_FAILURE);
    auto& slave_internal = candidate(
        report.candidates,
        RootCauseKind::SLAVE_INTERNAL_ERROR);
    auto& quality = candidate(
        report.candidates,
        RootCauseKind::LINK_QUALITY_DEGRADATION);

    std::optional<bool> latest_master_link;
    std::optional<AlEvidence> latest_al;
    std::map<int, SlaveStateEvidence> latest_slave_states;
    std::set<int> lost_slaves;
    std::set<std::string> collection_errors;

    for (const DiagnosisEvidence& frame : evidence)
    {
        report.timestamp_ms =
            std::max(report.timestamp_ms, frame.timestamp_ms);

        if (frame.master_link)
        {
            latest_master_link = frame.master_link->link_up;
        }
        if (frame.boundary)
        {
            report.boundary = frame.boundary;
        }
        if (frame.al_status)
        {
            latest_al = frame.al_status;
        }
        for (const SlaveLossEvidence& lost : frame.lost_slaves)
        {
            if (lost.slave_position >= 0)
            {
                lost_slaves.insert(lost.slave_position);
            }
        }
        for (const SlaveStateEvidence& state : frame.slave_states)
        {
            latest_slave_states[state.slave_position] = state;
        }
        for (const std::string& error : frame.collection_errors)
        {
            collection_errors.insert(error);
        }

    }

    // Score port facts only after the latest boundary has been found. A port
    // event may precede active diagnosis by one or more monitor cycles.
    for (const DiagnosisEvidence& frame : evidence)
    {
        for (const PortErrorEvidence& error : frame.port_errors)
        {
            if (error.delta <= 0)
            {
                continue;
            }

            const bool at_boundary = report.boundary &&
                error.slave_position ==
                    report.boundary->last_alive_slave;

            if (error.kind == PortErrorEvidenceKind::LOST_LINK)
            {
                if (at_boundary)
                {
                    support(
                        boundary_link,
                        std::min(
                            weights_.boundary_lost_link_cap,
                            error.delta *
                                weights_.boundary_lost_link_per_delta),
                        portPrefix(error) +
                            "lost-link counter increased by " +
                            std::to_string(error.delta));
                }
                support(
                    quality,
                    std::min(
                        weights_.quality_lost_link_cap,
                        error.delta *
                            weights_.quality_lost_link_per_delta),
                    portPrefix(error) +
                        "lost-link counter increased by " +
                        std::to_string(error.delta));
                continue;
            }

            std::string counter_name;
            switch (error.kind)
            {
            case PortErrorEvidenceKind::INVALID_FRAME:
                counter_name = "invalid-frame";
                break;
            case PortErrorEvidenceKind::RX_ERROR:
                counter_name = "receive-error";
                break;
            case PortErrorEvidenceKind::FORWARDED_RX_ERROR:
                counter_name = "forwarded receive-error";
                break;
            case PortErrorEvidenceKind::LOST_LINK:
                break;
            }

            const int weight = std::min(
                weights_.quality_error_cap,
                error.delta * weights_.quality_error_per_delta);
            support(
                quality,
                weight,
                portPrefix(error) + counter_name +
                    " counter increased by " +
                    std::to_string(error.delta));

            if (at_boundary)
            {
                support(
                    boundary_link,
                    std::min(
                        weights_.boundary_other_error_cap,
                        error.delta *
                            weights_.boundary_other_error_per_delta),
                    portPrefix(error) + counter_name +
                        " counter increased at fault boundary");
            }
        }
    }

    report.collection_errors.assign(
        collection_errors.begin(),
        collection_errors.end());

    if (latest_master_link)
    {
        if (*latest_master_link)
        {
            contradict(
                master,
                weights_.master_up_master_contradiction,
                "Latest Master link is UP");
            if (report.boundary ||
                !boundary_link.supporting_evidence.empty())
            {
                support(
                    boundary_link,
                    weights_.master_up_boundary,
                    "Master link remains UP");
            }
            if (!quality.supporting_evidence.empty())
            {
                support(
                    quality,
                    weights_.master_up_quality,
                    "Master link remains UP");
            }
        }
        else
        {
            support(
                master,
                weights_.master_link_down,
                "Master link is DOWN");
            contradict(
                boundary_link,
                weights_.master_down_boundary_contradiction,
                "Master link is DOWN, so a downstream boundary is less likely");
            contradict(
                slave_internal,
                weights_.master_down_slave_contradiction,
                "Master link is DOWN, so slave evidence may be secondary");
        }
    }

    if (report.boundary)
    {
        support(
            boundary_link,
            weights_.boundary_detected,
            "Fault boundary is Slave" +
                std::to_string(report.boundary->last_alive_slave) +
                "<->Slave" +
                std::to_string(report.boundary->first_lost_slave));
    }

    for (int position : lost_slaves)
    {
        support(
            boundary_link,
            weights_.slave_lost_boundary,
            "Slave" + std::to_string(position) + " is lost");
    }
    if (!lost_slaves.empty())
    {
        support(
            master,
            weights_.slave_loss_master,
            "One or more slaves disappeared");
        contradict(
            quality,
            weights_.slave_loss_quality_contradiction,
            "Slave loss indicates more than quality degradation");
    }
    else if (!quality.supporting_evidence.empty())
    {
        support(
            quality,
            weights_.no_slave_loss_quality,
            "No slave loss was observed");
    }

    if (latest_al)
    {
        if (latest_al->status_code)
        {
            if (*latest_al->status_code != 0U)
            {
                support(
                    slave_internal,
                    weights_.al_status_code_error,
                    "Slave" +
                        std::to_string(latest_al->slave_position) +
                        " AL Status Code=" +
                        hex16(*latest_al->status_code));
            }
            else
            {
                contradict(
                    slave_internal,
                    weights_.al_status_code_clear_contradiction,
                    "AL Status Code reports no error");
            }
        }
        if (latest_al->error_indication)
        {
            if (*latest_al->error_indication)
            {
                support(
                    slave_internal,
                    weights_.al_error_indication,
                    "AL error indication is set");
            }
            else
            {
                contradict(
                    slave_internal,
                    weights_.al_error_clear_contradiction,
                    "AL error indication is clear");
            }
        }
    }

    for (const auto& [position, state] : latest_slave_states)
    {
        if (state.has_error)
        {
            support(
                slave_internal,
                weights_.slave_state_error,
                "Slave" + std::to_string(position) +
                    " reports an AL error state");
        }
    }

    for (RootCauseCandidate& value : report.candidates)
    {
        value.score = std::max(0, value.score);
        installActions(value);
    }

    std::stable_sort(
        report.candidates.begin(),
        report.candidates.end(),
        [](const RootCauseCandidate& left,
           const RootCauseCandidate& right) {
            if (left.score != right.score)
            {
                return left.score > right.score;
            }
            return kindPriority(left.kind) < kindPriority(right.kind);
        });

    if (report.candidates.front().score == 0)
    {
        const auto unknown = std::find_if(
            report.candidates.begin(),
            report.candidates.end(),
            [](const RootCauseCandidate& value) {
                return value.kind == RootCauseKind::UNKNOWN;
            });
        std::rotate(report.candidates.begin(), unknown, unknown + 1);
    }

    report.primary = report.candidates.front();

    int total_positive = 0;
    for (const RootCauseCandidate& value : report.candidates)
    {
        if (value.kind != RootCauseKind::UNKNOWN)
        {
            total_positive += value.score;
        }
    }
    if (report.primary.kind != RootCauseKind::UNKNOWN &&
        total_positive > 0)
    {
        report.confidence =
            static_cast<double>(report.primary.score) /
            static_cast<double>(total_positive);
    }

    const int runner_up = report.candidates.size() > 1U
        ? report.candidates[1].score
        : 0;
    report.conclusive =
        report.primary.kind != RootCauseKind::UNKNOWN &&
        report.primary.score >= weights_.conclusive_min_score &&
        report.primary.score - runner_up >=
            weights_.conclusive_min_lead &&
        report.confidence >=
            weights_.conclusive_min_confidence;

    return report;
}
