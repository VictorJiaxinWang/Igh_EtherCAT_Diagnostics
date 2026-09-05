#include "ethercat_diag/root_cause/root_cause_calibration.h"

#include "ethercat_diag/root_cause/root_cause_formatter.h"

#include <iomanip>
#include <sstream>
#include <utility>

double FaultMatrixSummary::accuracy() const noexcept
{
    if (entries.empty())
    {
        return 0.0;
    }
    return static_cast<double>(passed) /
        static_cast<double>(entries.size());
}

RootCauseCalibration::RootCauseCalibration(
    RootCauseAnalyzer analyzer)
    : analyzer_(std::move(analyzer))
{
}

FaultMatrixSummary RootCauseCalibration::evaluate(
    const std::vector<FaultMatrixCase>& cases) const
{
    FaultMatrixSummary summary;
    summary.entries.reserve(cases.size());

    for (const FaultMatrixCase& test_case : cases)
    {
        const RootCauseReport report =
            analyzer_.analyze(test_case.evidence);
        const bool kind_matches =
            report.primary.kind == test_case.expected;
        const bool certainty_matches =
            test_case.expected == RootCauseKind::UNKNOWN ||
            report.conclusive;
        const bool passed = kind_matches && certainty_matches;

        summary.entries.push_back({
            test_case.name,
            test_case.expected,
            report.primary.kind,
            report.primary.score,
            report.confidence,
            report.conclusive,
            passed});
        if (passed)
        {
            ++summary.passed;
        }
    }
    return summary;
}

std::string formatFaultMatrixSummary(
    const FaultMatrixSummary& summary)
{
    std::ostringstream output;
    output << "[FAULT_MATRIX] passed=" << summary.passed
           << '/' << summary.entries.size()
           << " accuracy=" << std::fixed << std::setprecision(1)
           << summary.accuracy() * 100.0 << '%';

    for (const FaultMatrixEntry& entry : summary.entries)
    {
        output << "\n[" << (entry.passed ? "PASS" : "FAIL") << "] "
               << entry.name
               << " expected=" << rootCauseKindToString(entry.expected)
               << " actual=" << rootCauseKindToString(entry.actual)
               << " score=" << entry.score
               << " confidence=" << std::fixed << std::setprecision(2)
               << entry.confidence
               << " certainty="
               << (entry.conclusive ? "CONCLUSIVE" : "INCONCLUSIVE");
    }
    return output.str();
}
