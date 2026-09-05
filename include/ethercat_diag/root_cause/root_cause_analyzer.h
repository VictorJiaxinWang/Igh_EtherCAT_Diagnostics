#pragma once

#include "ethercat_diag/root_cause/root_cause_report.h"

#include <vector>

struct RootCauseRuleWeights
{
    int master_link_down{100};
    int slave_loss_master{10};
    int master_up_master_contradiction{80};

    int boundary_detected{10};
    int slave_lost_boundary{35};
    int master_up_boundary{15};
    int master_down_boundary_contradiction{60};
    int boundary_lost_link_per_delta{60};
    int boundary_lost_link_cap{60};
    int boundary_other_error_per_delta{3};
    int boundary_other_error_cap{20};

    int al_status_code_error{70};
    int al_error_indication{40};
    int slave_state_error{35};
    int master_down_slave_contradiction{25};
    int al_status_code_clear_contradiction{15};
    int al_error_clear_contradiction{10};

    int quality_error_per_delta{5};
    int quality_error_cap{30};
    int quality_lost_link_per_delta{5};
    int quality_lost_link_cap{10};
    int master_up_quality{10};
    int no_slave_loss_quality{15};
    int slave_loss_quality_contradiction{10};

    int conclusive_min_score{60};
    int conclusive_min_lead{15};
    double conclusive_min_confidence{0.55};
};

class RootCauseAnalyzer
{
public:
    explicit RootCauseAnalyzer(
        RootCauseRuleWeights weights = RootCauseRuleWeights{});

    RootCauseReport analyze(
        const std::vector<DiagnosisEvidence>& evidence) const;

    const RootCauseRuleWeights& weights() const noexcept;

private:
    RootCauseRuleWeights weights_;
};
