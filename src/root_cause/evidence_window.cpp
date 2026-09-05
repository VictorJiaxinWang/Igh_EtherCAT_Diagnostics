#include "ethercat_diag/root_cause/evidence_window.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace
{

bool isRelatedSlave(
    int slave_position,
    const BoundaryEvidence& boundary)
{
    if (slave_position < 0 ||
        boundary.first_lost_slave < 0)
    {
        return false;
    }

    if (boundary.last_alive_slave < 0)
    {
        return slave_position >= boundary.first_lost_slave;
    }

    return slave_position == boundary.last_alive_slave ||
        slave_position >= boundary.first_lost_slave;
}

bool sameBoundary(
    const BoundaryEvidence& left,
    const BoundaryEvidence& right)
{
    return left.last_alive_slave == right.last_alive_slave &&
        left.first_lost_slave == right.first_lost_slave;
}

template <typename Container, typename PositionFunction>
void removeUnrelated(
    Container& values,
    const BoundaryEvidence& boundary,
    PositionFunction position)
{
    values.erase(
        std::remove_if(
            values.begin(),
            values.end(),
            [&](const auto& value) {
                return !isRelatedSlave(position(value), boundary);
            }),
        values.end());
}

} // namespace

EvidenceWindow::EvidenceWindow(std::uint64_t duration_ms)
    : duration_ms_(duration_ms)
{
    if (duration_ms_ == 0U)
    {
        throw std::invalid_argument(
            "evidence window duration must be greater than zero");
    }
}

void EvidenceWindow::push(DiagnosisEvidence evidence)
{
    if (!frames_.empty() &&
        evidence.timestamp_ms < frames_.back().timestamp_ms)
    {
        frames_.clear();
    }

    const std::uint64_t latest = evidence.timestamp_ms;
    frames_.push_back(std::move(evidence));

    const std::uint64_t lower_bound =
        latest > duration_ms_
            ? latest - duration_ms_
            : 0U;

    while (!frames_.empty() &&
           frames_.front().timestamp_ms < lower_bound)
    {
        frames_.pop_front();
    }
}

const std::deque<DiagnosisEvidence>&
EvidenceWindow::recent() const noexcept
{
    return frames_;
}

std::vector<DiagnosisEvidence>
EvidenceWindow::relatedToBoundary(
    const BoundaryEvidence& boundary) const
{
    std::vector<DiagnosisEvidence> related;
    related.reserve(frames_.size());

    for (const DiagnosisEvidence& original : frames_)
    {
        DiagnosisEvidence filtered = original;

        if (filtered.boundary &&
            !sameBoundary(*filtered.boundary, boundary))
        {
            filtered.boundary.reset();
        }

        removeUnrelated(
            filtered.lost_slaves,
            boundary,
            [](const SlaveLossEvidence& value) {
                return value.slave_position;
            });
        removeUnrelated(
            filtered.port_errors,
            boundary,
            [](const PortErrorEvidence& value) {
                return value.slave_position;
            });
        removeUnrelated(
            filtered.slave_states,
            boundary,
            [](const SlaveStateEvidence& value) {
                return value.slave_position;
            });

        if (filtered.al_status &&
            !isRelatedSlave(
                filtered.al_status->slave_position,
                boundary))
        {
            filtered.al_status.reset();
        }

        if (filtered.port_snapshot &&
            !isRelatedSlave(
                filtered.port_snapshot->slave_position,
                boundary))
        {
            filtered.port_snapshot.reset();
        }

        related.push_back(std::move(filtered));
    }

    return related;
}
